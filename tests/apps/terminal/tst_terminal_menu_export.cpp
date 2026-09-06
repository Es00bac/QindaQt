// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalmenuappletcomposition.h"

#include <qindaqt/applet_host/capability_policy_loader.h>
#include <qindaqt/applets/manifest_catalog.h>
#include <qindaqt/compositor/shellwindowidentity.h>
#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>
#include <qindaqt/shell_window_actions_client/shell_window_actions_client.h>
#include <qindaqt/shell_window_actions_client/shell_window_actions_transport.h>

#include <QDBusContext>
#include <QDBusError>
#include <QDBusObjectPath>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QtTest>

#include <optional>

using namespace QindaQt;

namespace {

constexpr quint32 kTestWindowId = 77;
const auto kActivateId = QStringLiteral("session.new-tab");
const auto kQuitId = QStringLiteral("file.quit");
const auto kAllowedStderrPrefix = QStringLiteral(
    "qindaqt-terminal: settings unavailable");

class FakeIdentityTransport final
    : public ShellWindowActionsClient::ShellWindowActionsTransport {
public:
  bool start(QString *) override { return true; }
  void stop() override {}
  void request(quint64, const QString &, Compositor::ShellWindowAction,
               const QString &,
               const Compositor::ShellWindowGeneration &) override {}
  void requestIdentity(quint64 token, const QString &owner) override {
    m_request = qMakePair(token, owner);
  }

  void publishOwner(const QString &owner) { Q_EMIT serviceOwnerChanged(owner); }

  bool publishIdentity(qint64 processId, quint32 registrarWindowId) {
    if (!m_request) {
      return false;
    }
    const Compositor::ShellWindowIdentitySnapshot snapshot{
        Compositor::ShellWindowIdentityStatus::Ok,
        QStringLiteral("terminal-menu-export-identity-epoch"),
        1,
        {QStringLiteral("terminal-menu-export-action-epoch"), 1},
        Compositor::ShellWindowIdentityFacts{
            QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"), processId,
            registrarWindowId, std::nullopt, std::nullopt},
        {},
        {}};
    Q_EMIT identityReplyReceived(
        m_request->first, m_request->second,
        Compositor::encodeShellWindowIdentitySnapshot(snapshot));
    return true;
  }

private:
  std::optional<QPair<quint64, QString>> m_request;
};

// Owns the registrar well-known name but refuses every registration with a
// D-Bus error, proving the application-side export fails closed against a
// hostile registrar rather than publishing or crashing.
class HostileRegistrar final : public QObject, public QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "com.canonical.AppMenu.Registrar")

public:
  using QObject::QObject;

  int attempts = 0;

public Q_SLOTS:
  Q_SCRIPTABLE void RegisterWindow(quint32, const QDBusObjectPath &) {
    ++attempts;
    sendErrorReply(QDBusError::AccessDenied,
                   QStringLiteral("hostile registrar"));
  }
};

struct CatalogFixture final {
  Applets::ManifestCatalog catalog;
  AppletHost::CapabilityPolicy policy;

  bool load(QString *error) {
    if (!catalog.loadDirectory(
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), error)) {
      return false;
    }
    const auto loaded = AppletHost::CapabilityPolicyLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json"));
    if (!loaded.ok) {
      *error = loaded.error;
      return false;
    }
    policy = loaded.policy;
    return true;
  }
};

std::optional<QString> actionIdWithText(const QVariantList &items,
                                        const QString &text) {
  for (const QVariant &value : items) {
    const QVariantMap item = value.toMap();
    if (item.value(QStringLiteral("text")).toString() == text &&
        item.value(QStringLiteral("kind")).toString() ==
            QStringLiteral("action")) {
      return item.value(QStringLiteral("id")).toString();
    }
    if (const auto child = actionIdWithText(
            item.value(QStringLiteral("children")).toList(), text)) {
      return child;
    }
  }
  return std::nullopt;
}

qsizetype activationCount(const QByteArray &output, const QString &actionId) {
  return output.count(
      (QStringLiteral("ACTIVATED ") + actionId + QStringLiteral("\n"))
          .toUtf8());
}

QString unexpectedStderr(const QByteArray &standardError) {
  QString unexpected;
  const QStringList lines =
      QString::fromUtf8(standardError).split(QLatin1Char('\n'));
  for (const QString &line : lines) {
    if (line.isEmpty() || line.startsWith(kAllowedStderrPrefix) ||
        line == QStringLiteral(
            "This plugin does not support propagateSizeHints()")) {
      continue;
    }
    unexpected += line + QStringLiteral("\n");
  }
  return unexpected;
}

} // namespace

class TerminalMenuExportTest final : public QObject {
  Q_OBJECT

private:
  QString m_isolationRoot;

  void setIsolationRoot(const QString &name) {
    m_isolationRoot = QStringLiteral(QINDAQT_TEST_ISOLATION_ROOT) +
                      QStringLiteral("/") + name;
  }

  [[nodiscard]] QProcessEnvironment appEnvironment() const {
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"),
                       QStringLiteral("offscreen"));
    // AGENT-NOTE: no QT_FATAL_WARNINGS here, unlike the editor row. The real
    // qtermwidget surface under the offscreen QPA emits Qt's platform warning
    // "This plugin does not support propagateSizeHints()", which would abort
    // the real process before any menu evidence; the rendering adapter is
    // outside this lane and its registered rows also run without fatal
    // warnings.
    environment.insert(QStringLiteral("QT_FATAL_WARNINGS"), QStringLiteral("0"));
    environment.insert(QStringLiteral("QINDAQT_TEST_APPMENU_WINDOW_ID"),
                       QString::number(kTestWindowId));
    environment.insert(QStringLiteral("QINDAQT_TEST_APPMENU_TRACE_ACTIVATION"),
                       QStringLiteral("1"));
    // The hermetic shell resolution keeps the child PTY program independent
    // of the developer's login shell.
    environment.insert(QStringLiteral("SHELL"), QStringLiteral("/bin/sh"));
    for (const QString &name :
         {QStringLiteral("home"), QStringLiteral("state"),
          QStringLiteral("data"), QStringLiteral("tmp")}) {
      (void)QDir(m_isolationRoot + QLatin1Char('/') + name)
          .mkpath(QStringLiteral("."));
    }
    environment.insert(QStringLiteral("HOME"), m_isolationRoot + "/home");
    environment.insert(QStringLiteral("XDG_STATE_HOME"),
                       m_isolationRoot + "/state");
    environment.insert(QStringLiteral("XDG_DATA_HOME"),
                       m_isolationRoot + "/data");
    environment.insert(QStringLiteral("TMPDIR"), m_isolationRoot + "/tmp");
    environment.remove(QStringLiteral("DISPLAY"));
    environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
    return environment;
  }

  void launchTerminal(QProcess &terminal, QByteArray *standardOutput,
                      QByteArray *standardError) {
    connect(&terminal, &QProcess::readyReadStandardOutput, this,
            [standardOutput, &terminal] {
              standardOutput->append(terminal.readAllStandardOutput());
            });
    connect(&terminal, &QProcess::readyReadStandardError, this,
            [standardError, &terminal] {
              standardError->append(terminal.readAllStandardError());
            });
    terminal.setProcessEnvironment(appEnvironment());
    terminal.setProcessChannelMode(QProcess::SeparateChannels);
    terminal.setProgram(QStringLiteral(QINDAQT_TERMINAL));
    terminal.setArguments({QStringLiteral("--theme-directory"),
                           QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes")});
    terminal.start();
    QVERIFY2(terminal.waitForStarted(5'000),
             qPrintable(terminal.errorString()));
    QVERIFY(terminal.processId() > 0);
  }

  static void requestQuitThroughShellMenu(
      Shell::GlobalMenuAppletComposition &composition,
      QProcess &terminal) {
    const std::optional<QString> quitId = actionIdWithText(
        composition.access()->items(), QStringLiteral("Quit"));
    QVERIFY(quitId.has_value());
    composition.access()->activate(*quitId);
    if (!terminal.waitForFinished(10'000)) {
      QFAIL("the activated file.quit action did not end the terminal process");
    }
  }

private Q_SLOTS:
  void shellFencesRealTerminalIdentity_data();
  void shellFencesRealTerminalIdentity();
  void staysFailClosedUntilRegistrarArrives();
  void failsClosedUnderHostileRegistrar();
};

void TerminalMenuExportTest::shellFencesRealTerminalIdentity_data() {
  QTest::addColumn<int>("identityVariant");
  // AGENT-NOTE: regression proof mirroring the File Manager row. These must
  // remain real-process rows: same-process ownership fakes cannot prove the
  // final PID/window join.
  QTest::newRow("matching-pid-and-window") << 0;
  QTest::newRow("mismatched-pid") << 1;
  QTest::newRow("mismatched-window-id") << 2;
}

void TerminalMenuExportTest::shellFencesRealTerminalIdentity() {
  QFETCH(int, identityVariant);
  const QString tag = QString::fromLatin1(QTest::currentDataTag());
  setIsolationRoot(QStringLiteral("identity-") + tag);
  auto shellBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("terminal-menu-export-shell-") + tag);
  QVERIFY(shellBus.isConnected());

  CatalogFixture fixture;
  QString error;
  QVERIFY2(fixture.load(&error), qPrintable(error));
  FakeIdentityTransport transport;
  ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
  QVERIFY(client.start());
  transport.publishOwner(QStringLiteral(":1.900"));

  Shell::GlobalMenuAppletComposition composition(
      fixture.catalog, fixture.policy, shellBus, client);
  composition.start();
  QCOMPARE(composition.status(), Shell::GlobalMenuRuntimeStatus::Ready);

  QProcess terminal;
  auto processCleanup = qScopeGuard([&terminal] {
    if (terminal.state() == QProcess::NotRunning) {
      return;
    }
    terminal.terminate();
    if (!terminal.waitForFinished(2'000)) {
      terminal.kill();
      (void)terminal.waitForFinished(2'000);
    }
  });
  QByteArray standardOutput;
  QByteArray standardError;
  launchTerminal(terminal, &standardOutput, &standardError);
  const qint64 publishedPid = static_cast<qint64>(terminal.processId()) +
                              (identityVariant == 1 ? 1 : 0);
  const quint32 publishedWindowId =
      identityVariant == 2 ? kTestWindowId + 1 : kTestWindowId;
  QTRY_VERIFY_WITH_TIMEOUT(
      transport.publishIdentity(publishedPid, publishedWindowId), 5'000);
  QTRY_VERIFY_WITH_TIMEOUT(client.identityAvailable(), 5'000);
  if (identityVariant == 0) {
    QTRY_VERIFY_WITH_TIMEOUT(composition.access()->available(), 10'000);
    const std::optional<QString> actionId = actionIdWithText(
        composition.access()->items(), QStringLiteral("New Tab"));
    QVERIFY(actionId.has_value());
    composition.access()->activate(*actionId);
    QTRY_COMPARE_WITH_TIMEOUT(
        activationCount(standardOutput, kActivateId), qsizetype{1}, 5'000);
    QTest::qWait(150);
    QCOMPARE(activationCount(standardOutput, kActivateId), qsizetype{1});
    // The activated file.quit ends the provider through the real close path;
    // the shell must clear its menu when the surface retires.
    requestQuitThroughShellMenu(composition, terminal);
    processCleanup.dismiss();
  } else {
    QTest::qWait(1'000);
    QVERIFY(!composition.access()->available());
    // The retained placeholder clears after the bounded presentation grace once
  // no provider re-proves the endpoint.
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->items().isEmpty(), 5'000);
    composition.access()->activate(QStringLiteral("1"));
    QTest::qWait(250);
    QCOMPARE(activationCount(standardOutput, kActivateId), qsizetype{0});
    QCOMPARE(activationCount(standardOutput, kQuitId), qsizetype{0});
    QVERIFY(terminal.state() != QProcess::NotRunning);
    terminal.terminate();
    if (!terminal.waitForFinished(5'000)) {
      terminal.kill();
      QVERIFY(terminal.waitForFinished(5'000));
    }
    processCleanup.dismiss();
  }
  QTRY_VERIFY_WITH_TIMEOUT(!composition.access()->available(), 5'000);
  // The retained placeholder clears after the bounded presentation grace once
  // no provider re-proves the endpoint.
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->items().isEmpty(), 5'000);
  standardOutput.append(terminal.readAllStandardOutput());
  standardError.append(terminal.readAllStandardError());
  QVERIFY2(unexpectedStderr(standardError).isEmpty(),
           qPrintable(unexpectedStderr(standardError)));

  composition.stop();
  client.stop();
  QDBusConnection::disconnectFromBus(
      QStringLiteral("terminal-menu-export-shell-") + tag);
}

void TerminalMenuExportTest::staysFailClosedUntilRegistrarArrives() {
  setIsolationRoot(QStringLiteral("registrar-absent"));
  auto shellBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("terminal-menu-export-absent-shell"));
  QVERIFY(shellBus.isConnected());

  QProcess terminal;
  auto processCleanup = qScopeGuard([&terminal] {
    if (terminal.state() == QProcess::NotRunning) {
      return;
    }
    terminal.terminate();
    if (!terminal.waitForFinished(2'000)) {
      terminal.kill();
      (void)terminal.waitForFinished(2'000);
    }
  });
  QByteArray standardOutput;
  QByteArray standardError;
  // No registrar exists on this bus yet. The terminal must keep running with
  // its local QMenuBar as the only authority and without any failure noise.
  launchTerminal(terminal, &standardOutput, &standardError);
  QTest::qWait(1'500);
  QVERIFY2(terminal.state() != QProcess::NotRunning,
           "the terminal exited while no registrar existed");

  CatalogFixture fixture;
  QString error;
  QVERIFY2(fixture.load(&error), qPrintable(error));
  FakeIdentityTransport transport;
  ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
  QVERIFY(client.start());
  transport.publishOwner(QStringLiteral(":1.900"));
  Shell::GlobalMenuAppletComposition composition(
      fixture.catalog, fixture.policy, shellBus, client);
  composition.start();
  QCOMPARE(composition.status(), Shell::GlobalMenuRuntimeStatus::Ready);
  QTRY_VERIFY_WITH_TIMEOUT(
      transport.publishIdentity(static_cast<qint64>(terminal.processId()),
                                kTestWindowId),
      5'000);
  QTRY_VERIFY_WITH_TIMEOUT(client.identityAvailable(), 5'000);
  // AGENT-NOTE: late binding proves the export was genuinely waiting on the
  // registrar owner rather than silently disabled at startup.
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->available(), 10'000);
  requestQuitThroughShellMenu(composition, terminal);
  processCleanup.dismiss();
  QTRY_VERIFY_WITH_TIMEOUT(!composition.access()->available(), 5'000);
  // The retained placeholder clears after the bounded presentation grace once
  // no provider re-proves the endpoint.
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->items().isEmpty(), 5'000);
  standardOutput.append(terminal.readAllStandardOutput());
  standardError.append(terminal.readAllStandardError());
  QVERIFY2(unexpectedStderr(standardError).isEmpty(),
           qPrintable(unexpectedStderr(standardError)));

  composition.stop();
  client.stop();
  QDBusConnection::disconnectFromBus(
      QStringLiteral("terminal-menu-export-absent-shell"));
}

void TerminalMenuExportTest::failsClosedUnderHostileRegistrar() {
  setIsolationRoot(QStringLiteral("hostile-registrar"));
  auto shellBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("terminal-menu-export-hostile-shell"));
  QVERIFY(shellBus.isConnected());

  HostileRegistrar hostile;
  QVERIFY(shellBus.registerService(
      QStringLiteral("com.canonical.AppMenu.Registrar")));
  QVERIFY(shellBus.registerObject(
      QStringLiteral("/com/canonical/AppMenu/Registrar"), &hostile,
      QDBusConnection::ExportScriptableSlots));

  QProcess terminal;
  auto processCleanup = qScopeGuard([&terminal] {
    if (terminal.state() == QProcess::NotRunning) {
      return;
    }
    terminal.terminate();
    if (!terminal.waitForFinished(2'000)) {
      terminal.kill();
      (void)terminal.waitForFinished(2'000);
    }
  });
  QByteArray standardOutput;
  QByteArray standardError;
  launchTerminal(terminal, &standardOutput, &standardError);
  // The export really attempted registration against the hostile owner and
  // was refused; the application stays alive with its local menu.
  QTRY_VERIFY_WITH_TIMEOUT(hostile.attempts >= 1, 5'000);
  QTest::qWait(500);
  QVERIFY2(terminal.state() != QProcess::NotRunning,
           "the terminal exited after a hostile registrar refusal");

  shellBus.unregisterObject(
      QStringLiteral("/com/canonical/AppMenu/Registrar"));
  QVERIFY(shellBus.unregisterService(
      QStringLiteral("com.canonical.AppMenu.Registrar")));

  CatalogFixture fixture;
  QString error;
  QVERIFY2(fixture.load(&error), qPrintable(error));
  FakeIdentityTransport transport;
  ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
  QVERIFY(client.start());
  transport.publishOwner(QStringLiteral(":1.900"));
  Shell::GlobalMenuAppletComposition composition(
      fixture.catalog, fixture.policy, shellBus, client);
  composition.start();
  QCOMPARE(composition.status(), Shell::GlobalMenuRuntimeStatus::Ready);
  QTRY_VERIFY_WITH_TIMEOUT(
      transport.publishIdentity(static_cast<qint64>(terminal.processId()),
                                kTestWindowId),
      5'000);
  QTRY_VERIFY_WITH_TIMEOUT(client.identityAvailable(), 5'000);
  // AGENT-NOTE: rebinding after the hostile owner is replaced proves the
  // refusal degraded to waiting, not to a permanent failure.
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->available(), 10'000);
  requestQuitThroughShellMenu(composition, terminal);
  processCleanup.dismiss();
  QTRY_VERIFY_WITH_TIMEOUT(!composition.access()->available(), 5'000);
  // The retained placeholder clears after the bounded presentation grace once
  // no provider re-proves the endpoint.
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->items().isEmpty(), 5'000);
  standardOutput.append(terminal.readAllStandardOutput());
  standardError.append(terminal.readAllStandardError());
  QVERIFY2(unexpectedStderr(standardError).isEmpty(),
           qPrintable(unexpectedStderr(standardError)));

  composition.stop();
  client.stop();
  QDBusConnection::disconnectFromBus(
      QStringLiteral("terminal-menu-export-hostile-shell"));
}

QTEST_GUILESS_MAIN(TerminalMenuExportTest)
#include "tst_terminal_menu_export.moc"
