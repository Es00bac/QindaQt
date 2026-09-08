// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalmenuappletcomposition.h"

#include <qindaqt/applet_host/capability_policy_loader.h>
#include <qindaqt/applets/manifest_catalog.h>
#include <qindaqt/compositor/shellwindowidentity.h>
#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>
#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_wire.h>
#include <qindaqt/shell_window_actions_client/shell_window_actions_client.h>
#include <qindaqt/shell_window_actions_client/shell_window_actions_transport.h>

#include <QDBusContext>
#include <QDBusError>
#include <QDBusPendingReply>
#include <QFile>
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
const auto kActivateId = QStringLiteral("view.word-wrap");
const auto kAllowedStderrPrefix = QStringLiteral(
    "qindaqt-editor: settings unavailable");

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
        QStringLiteral("editor-menu-export-identity-epoch"),
        1,
        {QStringLiteral("editor-menu-export-action-epoch"), 1},
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

class RecordingRegistrar final : public QObject, public QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "com.canonical.AppMenu.Registrar")
public:
  QStringList owners;
public Q_SLOTS:
  Q_SCRIPTABLE void RegisterWindow(quint32, const QDBusObjectPath &) {
    if (!owners.contains(message().service())) owners.append(message().service());
  }
  Q_SCRIPTABLE void UnregisterWindow(quint32) {}
  Q_SCRIPTABLE bool IsMenuHosted(const QString &, const QDBusObjectPath &) { return false; }
};

using Shell::GlobalMenu::DbusMenu::LayoutItem;
std::optional<LayoutItem> remoteLayout(const QDBusConnection &bus, const QString &owner) {
  auto call = QDBusMessage::createMethodCall(owner, QStringLiteral("/org/qindaqt/AppShell/Menu"),
      QStringLiteral("com.canonical.dbusmenu"), QStringLiteral("GetLayout"));
  call.setArguments({qint32(0), qint32(-1), QStringList{}});
  QDBusPendingReply<quint32, LayoutItem> reply = bus.asyncCall(call, 2000);
  reply.waitForFinished();
  if (!reply.isValid()) return std::nullopt;
  return reply.argumentAt<1>();
}
std::optional<LayoutItem> layoutAction(const LayoutItem &root, const QString &label) {
  if (root.properties.value(QStringLiteral("label")).toString() == label) return root;
  for (const auto &child : root.children) {
    const auto node = qdbus_cast<LayoutItem>(child);
    if (auto found = layoutAction(node, label)) return found;
  }
  return std::nullopt;
}
bool clickRemote(const QDBusConnection &bus, const QString &owner, qint32 id) {
  auto call = QDBusMessage::createMethodCall(owner, QStringLiteral("/org/qindaqt/AppShell/Menu"),
      QStringLiteral("com.canonical.dbusmenu"), QStringLiteral("Event"));
  call.setArguments({id, QStringLiteral("clicked"), QVariant::fromValue(QDBusVariant(0)), quint32(1)});
  return bus.call(call, QDBus::Block, 2000).type() == QDBusMessage::ReplyMessage;
}

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
    if (line.isEmpty() || line.startsWith(kAllowedStderrPrefix)) {
      continue;
    }
    unexpected += line + QStringLiteral("\n");
  }
  return unexpected;
}

} // namespace

class EditorMenuExportTest final : public QObject {
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
    environment.insert(QStringLiteral("QT_FATAL_WARNINGS"), QStringLiteral("1"));
    environment.insert(QStringLiteral("QINDAQT_TEST_APPMENU_WINDOW_ID"),
                       QString::number(kTestWindowId));
    environment.insert(QStringLiteral("QINDAQT_TEST_APPMENU_TRACE_ACTIVATION"),
                       QStringLiteral("1"));
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

  void launchEditor(QProcess &editor, QByteArray *standardOutput,
                    QByteArray *standardError, const QStringList &paths = {}) {
    connect(&editor, &QProcess::readyReadStandardOutput, this,
            [standardOutput, &editor] {
              standardOutput->append(editor.readAllStandardOutput());
            });
    connect(&editor, &QProcess::readyReadStandardError, this,
            [standardError, &editor] {
              standardError->append(editor.readAllStandardError());
            });
    editor.setProcessEnvironment(appEnvironment());
    editor.setProcessChannelMode(QProcess::SeparateChannels);
    editor.setProgram(QStringLiteral(QINDAQT_EDITOR));
    editor.setArguments(QStringList{QStringLiteral("--theme-directory"),
                         QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes")} + paths);
    editor.start();
    QVERIFY2(editor.waitForStarted(5'000), qPrintable(editor.errorString()));
    QVERIFY(editor.processId() > 0);
  }

  static void requestQuitThroughShellMenu(
      Shell::GlobalMenuAppletComposition &composition, QProcess &editor) {
    const std::optional<QString> quitId =
        actionIdWithText(composition.access()->items(), QStringLiteral("Quit"));
    QVERIFY(quitId.has_value());
    composition.access()->activate(*quitId);
    if (!editor.waitForFinished(10'000)) {
      QFAIL("the activated file.quit action did not end the editor process");
    }
  }

private Q_SLOTS:
  void shellFencesRealEditorIdentity_data();
  void shellFencesRealEditorIdentity();
  void staysFailClosedUntilRegistrarArrives();
  void failsClosedUnderHostileRegistrar();
  void eachDocumentOwnsItsMenuConnection();
};

void EditorMenuExportTest::eachDocumentOwnsItsMenuConnection() {
  setIsolationRoot(QStringLiteral("multiple-windows"));
  QVERIFY(QDir().mkpath(m_isolationRoot));
  QStringList paths;
  for (const QString &name : {QStringLiteral("first.txt"), QStringLiteral("second.txt")}) {
    paths.append(m_isolationRoot + QLatin1Char('/') + name);
    QFile file(paths.last()); QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("document"), qint64(8));
  }
  Shell::GlobalMenu::DbusMenu::registerDbusMenuWireTypes();
  auto bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
      QStringLiteral("editor-multiple-window-observer"));
  QVERIFY(bus.isConnected());
  RecordingRegistrar registrar;
  QVERIFY(bus.registerObject(QStringLiteral("/com/canonical/AppMenu/Registrar"), &registrar,
                              QDBusConnection::ExportScriptableSlots));
  QVERIFY(bus.registerService(QStringLiteral("com.canonical.AppMenu.Registrar")));
  QProcess editor;
  auto cleanup = qScopeGuard([&] {
    if (editor.state() != QProcess::NotRunning) {
      editor.terminate();
      if (!editor.waitForFinished(2000)) { editor.kill(); (void)editor.waitForFinished(2000); }
    }
    bus.unregisterService(QStringLiteral("com.canonical.AppMenu.Registrar"));
    QDBusConnection::disconnectFromBus(QStringLiteral("editor-multiple-window-observer"));
  });
  QByteArray out, err;
  launchEditor(editor, &out, &err, paths);
  QTRY_COMPARE_WITH_TIMEOUT(registrar.owners.size(), 2, 5000);
  const auto one = registrar.owners.at(0), two = registrar.owners.at(1);
  QVERIFY(one != two);
  // The offscreen fixed-ID publisher has no compositor identity. Assert actual
  // production endpoint independence through each registrar-announced owner.
  const auto first = remoteLayout(bus, one), second = remoteLayout(bus, two);
  QVERIFY(first); QVERIFY(second);
  auto firstWrap = layoutAction(*first, QStringLiteral("Word Wrap"));
  auto secondWrap = layoutAction(*second, QStringLiteral("Word Wrap"));
  QVERIFY(firstWrap); QVERIFY(secondWrap);
  QVERIFY(clickRemote(bus, one, firstWrap->id));
  auto updatedOne = remoteLayout(bus, one), updatedTwo = remoteLayout(bus, two);
  QVERIFY(updatedOne); QVERIFY(updatedTwo);
  QCOMPARE(layoutAction(*updatedOne, QStringLiteral("Word Wrap"))->properties.value("toggle-state").toInt(), 0);
  QCOMPARE(layoutAction(*updatedTwo, QStringLiteral("Word Wrap"))->properties.value("toggle-state").toInt(), 1);
  const auto secondQuit = layoutAction(*updatedTwo, QStringLiteral("Quit"));
  QVERIFY(secondQuit); QVERIFY(clickRemote(bus, two, secondQuit->id));
  QTRY_VERIFY_WITH_TIMEOUT(!remoteLayout(bus, two), 5000);
  QVERIFY(editor.state() != QProcess::NotRunning);
  updatedOne = remoteLayout(bus, one); QVERIFY(updatedOne);
  firstWrap = layoutAction(*updatedOne, QStringLiteral("Word Wrap"));
  QVERIFY(firstWrap); QVERIFY(clickRemote(bus, one, firstWrap->id));
  updatedOne = remoteLayout(bus, one); QVERIFY(updatedOne);
  QCOMPARE(layoutAction(*updatedOne, QStringLiteral("Word Wrap"))->properties.value("toggle-state").toInt(), 1);
  const auto firstQuit = layoutAction(*updatedOne, QStringLiteral("Quit"));
  QVERIFY(firstQuit); QVERIFY(clickRemote(bus, one, firstQuit->id));
  QTRY_COMPARE_WITH_TIMEOUT(editor.state(), QProcess::NotRunning, 5000);
  QCOMPARE(editor.exitCode(), 0);
  err.append(editor.readAllStandardError());
  QVERIFY2(unexpectedStderr(err).isEmpty(), qPrintable(unexpectedStderr(err)));
}

void EditorMenuExportTest::shellFencesRealEditorIdentity_data() {
  QTest::addColumn<int>("identityVariant");
  // AGENT-NOTE: regression proof mirroring the File Manager row. These must
  // remain real-process rows: same-process ownership fakes cannot prove the
  // final PID/window join.
  QTest::newRow("matching-pid-and-window") << 0;
  QTest::newRow("mismatched-pid") << 1;
  QTest::newRow("mismatched-window-id") << 2;
}

void EditorMenuExportTest::shellFencesRealEditorIdentity() {
  QFETCH(int, identityVariant);
  const QString tag = QString::fromLatin1(QTest::currentDataTag());
  setIsolationRoot(QStringLiteral("identity-") + tag);
  auto shellBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("editor-menu-export-shell-") + tag);
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

  QProcess editor;
  auto processCleanup = qScopeGuard([&editor] {
    if (editor.state() == QProcess::NotRunning) {
      return;
    }
    editor.terminate();
    if (!editor.waitForFinished(2'000)) {
      editor.kill();
      (void)editor.waitForFinished(2'000);
    }
  });
  QByteArray standardOutput;
  QByteArray standardError;
  launchEditor(editor, &standardOutput, &standardError);
  const qint64 publishedPid =
      static_cast<qint64>(editor.processId()) + (identityVariant == 1 ? 1 : 0);
  const quint32 publishedWindowId =
      identityVariant == 2 ? kTestWindowId + 1 : kTestWindowId;
  QTRY_VERIFY_WITH_TIMEOUT(
      transport.publishIdentity(publishedPid, publishedWindowId), 5'000);
  QTRY_VERIFY_WITH_TIMEOUT(client.identityAvailable(), 5'000);
  if (identityVariant == 0) {
    QTRY_VERIFY_WITH_TIMEOUT(composition.access()->available(), 10'000);
    const std::optional<QString> actionId = actionIdWithText(
        composition.access()->items(), QStringLiteral("Word Wrap"));
    QVERIFY(actionId.has_value());
    composition.access()->activate(*actionId);
    QTRY_COMPARE_WITH_TIMEOUT(
        activationCount(standardOutput, kActivateId), qsizetype{1}, 5'000);
    QTest::qWait(150);
    QCOMPARE(activationCount(standardOutput, kActivateId), qsizetype{1});
    // The activated file.quit ends the provider through the real close path;
    // the shell must clear its menu when the surface retires.
    requestQuitThroughShellMenu(composition, editor);
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
    QVERIFY(editor.state() != QProcess::NotRunning);
    editor.terminate();
    if (!editor.waitForFinished(5'000)) {
      editor.kill();
      QVERIFY(editor.waitForFinished(5'000));
    }
    processCleanup.dismiss();
  }
  QTRY_VERIFY_WITH_TIMEOUT(!composition.access()->available(), 5'000);
  // The retained placeholder clears after the bounded presentation grace once
  // no provider re-proves the endpoint.
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->items().isEmpty(), 5'000);
  standardOutput.append(editor.readAllStandardOutput());
  standardError.append(editor.readAllStandardError());
  QVERIFY2(unexpectedStderr(standardError).isEmpty(),
           qPrintable(unexpectedStderr(standardError)));

  composition.stop();
  client.stop();
  QDBusConnection::disconnectFromBus(
      QStringLiteral("editor-menu-export-shell-") + tag);
}

void EditorMenuExportTest::staysFailClosedUntilRegistrarArrives() {
  setIsolationRoot(QStringLiteral("registrar-absent"));
  auto shellBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("editor-menu-export-absent-shell"));
  QVERIFY(shellBus.isConnected());

  QProcess editor;
  auto processCleanup = qScopeGuard([&editor] {
    if (editor.state() == QProcess::NotRunning) {
      return;
    }
    editor.terminate();
    if (!editor.waitForFinished(2'000)) {
      editor.kill();
      (void)editor.waitForFinished(2'000);
    }
  });
  QByteArray standardOutput;
  QByteArray standardError;
  // No registrar exists on this bus yet. The editor must keep running with
  // its local QMenuBar as the only authority and without any failure noise.
  launchEditor(editor, &standardOutput, &standardError);
  QTest::qWait(1'500);
  QVERIFY2(editor.state() != QProcess::NotRunning,
           "the editor exited while no registrar existed");

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
      transport.publishIdentity(static_cast<qint64>(editor.processId()),
                                kTestWindowId),
      5'000);
  QTRY_VERIFY_WITH_TIMEOUT(client.identityAvailable(), 5'000);
  // AGENT-NOTE: late binding proves the export was genuinely waiting on the
  // registrar owner rather than silently disabled at startup.
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->available(), 10'000);
  requestQuitThroughShellMenu(composition, editor);
  processCleanup.dismiss();
  QTRY_VERIFY_WITH_TIMEOUT(!composition.access()->available(), 5'000);
  // The retained placeholder clears after the bounded presentation grace once
  // no provider re-proves the endpoint.
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->items().isEmpty(), 5'000);
  standardOutput.append(editor.readAllStandardOutput());
  standardError.append(editor.readAllStandardError());
  QVERIFY2(unexpectedStderr(standardError).isEmpty(),
           qPrintable(unexpectedStderr(standardError)));

  composition.stop();
  client.stop();
  QDBusConnection::disconnectFromBus(
      QStringLiteral("editor-menu-export-absent-shell"));
}

void EditorMenuExportTest::failsClosedUnderHostileRegistrar() {
  setIsolationRoot(QStringLiteral("hostile-registrar"));
  auto shellBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("editor-menu-export-hostile-shell"));
  QVERIFY(shellBus.isConnected());

  HostileRegistrar hostile;
  QVERIFY(shellBus.registerService(
      QStringLiteral("com.canonical.AppMenu.Registrar")));
  QVERIFY(shellBus.registerObject(
      QStringLiteral("/com/canonical/AppMenu/Registrar"), &hostile,
      QDBusConnection::ExportScriptableSlots));

  QProcess editor;
  auto processCleanup = qScopeGuard([&editor] {
    if (editor.state() == QProcess::NotRunning) {
      return;
    }
    editor.terminate();
    if (!editor.waitForFinished(2'000)) {
      editor.kill();
      (void)editor.waitForFinished(2'000);
    }
  });
  QByteArray standardOutput;
  QByteArray standardError;
  launchEditor(editor, &standardOutput, &standardError);
  // The export really attempted registration against the hostile owner and
  // was refused; the application stays alive with its local menu.
  QTRY_VERIFY_WITH_TIMEOUT(hostile.attempts >= 1, 5'000);
  QTest::qWait(500);
  QVERIFY2(editor.state() != QProcess::NotRunning,
           "the editor exited after a hostile registrar refusal");

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
      transport.publishIdentity(static_cast<qint64>(editor.processId()),
                                kTestWindowId),
      5'000);
  QTRY_VERIFY_WITH_TIMEOUT(client.identityAvailable(), 5'000);
  // AGENT-NOTE: rebinding after the hostile owner is replaced proves the
  // refusal degraded to waiting, not to a permanent failure.
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->available(), 10'000);
  requestQuitThroughShellMenu(composition, editor);
  processCleanup.dismiss();
  QTRY_VERIFY_WITH_TIMEOUT(!composition.access()->available(), 5'000);
  // The retained placeholder clears after the bounded presentation grace once
  // no provider re-proves the endpoint.
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->items().isEmpty(), 5'000);
  standardOutput.append(editor.readAllStandardOutput());
  standardError.append(editor.readAllStandardError());
  QVERIFY2(unexpectedStderr(standardError).isEmpty(),
           qPrintable(unexpectedStderr(standardError)));

  composition.stop();
  client.stop();
  QDBusConnection::disconnectFromBus(
      QStringLiteral("editor-menu-export-hostile-shell"));
}

QTEST_GUILESS_MAIN(EditorMenuExportTest)
#include "tst_editor_menu_export.moc"
