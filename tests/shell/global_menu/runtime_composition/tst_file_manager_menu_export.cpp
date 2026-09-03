// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalmenuappletcomposition.h"

#include <qindaqt/applet_host/capability_policy_loader.h>
#include <qindaqt/applets/manifest_catalog.h>
#include <qindaqt/compositor/shellwindowidentity.h>
#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>
#include <qindaqt/shell_window_actions_client/shell_window_actions_client.h>
#include <qindaqt/shell_window_actions_client/shell_window_actions_transport.h>

#include <QCoreApplication>
#include <QProcess>
#include <QScopeGuard>
#include <QtTest>

#include <optional>

using namespace QindaQt;

namespace {

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
        QStringLiteral("file-manager-test-identity-epoch"),
        1,
        {QStringLiteral("file-manager-test-action-epoch"), 1},
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

qsizetype activationCount(const QByteArray &output) {
  return output.count("ACTIVATED file.new-folder\n");
}

} // namespace

class FileManagerMenuExportTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void shellConsumesRealFileManagerAndActivatesExactlyOnce();
};

void FileManagerMenuExportTest::
    shellConsumesRealFileManagerAndActivatesExactlyOnce() {
  auto shellBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("file-manager-menu-export-shell"));
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

  QProcess fileManager;
  auto processCleanup = qScopeGuard([&fileManager] {
    if (fileManager.state() == QProcess::NotRunning) {
      return;
    }
    fileManager.terminate();
    if (!fileManager.waitForFinished(2'000)) {
      fileManager.kill();
      (void)fileManager.waitForFinished(2'000);
    }
  });
  QByteArray standardOutput;
  QByteArray standardError;
  connect(&fileManager, &QProcess::readyReadStandardOutput, this,
          [&] { standardOutput.append(fileManager.readAllStandardOutput()); });
  connect(&fileManager, &QProcess::readyReadStandardError, this,
          [&] { standardError.append(fileManager.readAllStandardError()); });
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("QT_QPA_PLATFORM"),
                     QStringLiteral("offscreen"));
  environment.insert(QStringLiteral("QT_QUICK_BACKEND"),
                     QStringLiteral("software"));
  environment.insert(QStringLiteral("QT_FATAL_WARNINGS"), QStringLiteral("1"));
  environment.insert(QStringLiteral("QINDAQT_TEST_APPMENU_WINDOW_ID"),
                     QStringLiteral("77"));
  environment.insert(QStringLiteral("QINDAQT_TEST_APPMENU_TRACE_ACTIVATION"),
                     QStringLiteral("1"));
  environment.insert(QStringLiteral("XDG_DATA_HOME"),
                     QStringLiteral(QINDAQT_TEST_DATA_DIR));
  environment.remove(QStringLiteral("DISPLAY"));
  environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
  fileManager.setProcessEnvironment(environment);
  fileManager.setProcessChannelMode(QProcess::SeparateChannels);
  fileManager.setProgram(QStringLiteral(QINDAQT_FILE_MANAGER));
  fileManager.setArguments({QStringLiteral("--theme-directory"),
                            QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"),
                            QStringLiteral(QINDAQT_BUILD_DIR)});
  fileManager.start();
  QVERIFY2(fileManager.waitForStarted(5'000),
           qPrintable(fileManager.errorString()));
  QVERIFY(fileManager.processId() > 0);
  QVERIFY(transport.publishIdentity(
      static_cast<qint64>(fileManager.processId()), 77));
  QTRY_VERIFY_WITH_TIMEOUT(client.identityAvailable(), 5'000);
  QTRY_VERIFY_WITH_TIMEOUT(composition.access()->available(), 10'000);

  const std::optional<QString> actionId = actionIdWithText(
      composition.access()->items(), QStringLiteral("New Folder"));
  QVERIFY(actionId.has_value());
  composition.access()->activate(*actionId);
  QTRY_COMPARE_WITH_TIMEOUT(activationCount(standardOutput), qsizetype{1},
                            5'000);
  QTest::qWait(150);
  QCOMPARE(activationCount(standardOutput), qsizetype{1});
  QVERIFY(fileManager.state() != QProcess::NotRunning);

  fileManager.terminate();
  if (!fileManager.waitForFinished(5'000)) {
    fileManager.kill();
    QVERIFY(fileManager.waitForFinished(5'000));
  }
  processCleanup.dismiss();
  QTRY_VERIFY_WITH_TIMEOUT(!composition.access()->available(), 5'000);
  QVERIFY(composition.access()->items().isEmpty());
  standardOutput.append(fileManager.readAllStandardOutput());
  standardError.append(fileManager.readAllStandardError());
  QVERIFY2(standardError.isEmpty(), standardError.constData());

  composition.stop();
  client.stop();
  QDBusConnection::disconnectFromBus(
      QStringLiteral("file-manager-menu-export-shell"));
}

QTEST_GUILESS_MAIN(FileManagerMenuExportTest)
#include "tst_file_manager_menu_export.moc"
