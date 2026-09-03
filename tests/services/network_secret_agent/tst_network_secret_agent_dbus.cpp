// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_secret_agent/resident_secret_agent.h>
#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include <QtCore/QProcess>
#include <QtCore/QThread>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusReply>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <memory>

using namespace QindaQt::Network::SecretAgent;

namespace {

class FakeAgentManager final : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.AgentManager")

public Q_SLOTS:
  void Register(const QString &identifier) {
    Q_EMIT registered(identifier, message().service());
  }
  void Unregister() { Q_EMIT unregistered(message().service()); }

Q_SIGNALS:
  void registered(const QString &identifier, const QString &caller);
  void unregistered(const QString &caller);
};

class FakeSettings final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.Settings")

public Q_SLOTS:
  QList<QDBusObjectPath> ListConnections() const {
    return {QDBusObjectPath(
        QStringLiteral("/org/freedesktop/NetworkManager/Settings/7"))};
  }
};

class FakePrompt final : public PromptPort {
public:
  QList<PromptRequest> requests;
  QHash<quint64, Completion> completions;
  QList<quint64> canceled;

  bool showPrompt(const PromptRequest &request,
                  Completion completion) override {
    requests.append(request);
    completions.insert(request.requestId, std::move(completion));
    return true;
  }

  void cancelPrompt(const quint64 requestId) override {
    canceled.append(requestId);
    completions.remove(requestId);
  }

  void submit(const bool remember) {
    const PromptRequest request = requests.last();
    auto completion = completions.take(request.requestId);
    completion(
        {request.requestId,
         true,
         remember,
         {{request.fields.first().key, QByteArray("private-bus-canary")}}});
  }
};

NmSettingsMap connectionMap() {
  return {{QStringLiteral("connection"),
           {{QStringLiteral("id"), QStringLiteral("Private Bus Wi-Fi")},
            {QStringLiteral("uuid"),
             QStringLiteral("12345678-1234-4234-9234-123456789abc")}}},
          {QStringLiteral("802-11-wireless-security"),
           {{QStringLiteral("key-mgmt"), QStringLiteral("wpa-psk")}}}};
}

QDBusMessage getSecretsMessage(const QString &destination, const QString &path,
                               const quint32 flags = 0x1U) {
  QDBusMessage message = QDBusMessage::createMethodCall(
      destination, QString::fromLatin1(kSecretAgentPath),
      QString::fromLatin1(kSecretAgentInterface), QStringLiteral("GetSecrets"));
  message.setArguments({QVariant::fromValue(connectionMap()),
                        QDBusObjectPath(path),
                        QStringLiteral("802-11-wireless-security"),
                        QStringList{QStringLiteral("psk")}, flags});
  return message;
}

QDBusMessage storageMessage(const QString &destination, const QString &method) {
  QDBusMessage message = QDBusMessage::createMethodCall(
      destination, QString::fromLatin1(kSecretAgentPath),
      QString::fromLatin1(kSecretAgentInterface), method);
  message.setArguments({QVariant::fromValue(connectionMap()),
                        QDBusObjectPath(QStringLiteral(
                            "/org/freedesktop/NetworkManager/Settings/7"))});
  return message;
}

std::unique_ptr<QDBusPendingCallWatcher> watch(const QDBusConnection &caller,
                                               const QDBusMessage &message) {
  return std::make_unique<QDBusPendingCallWatcher>(caller.asyncCall(message));
}

} // namespace

class NetworkSecretAgentDbusTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void registersAndUnregistersWithTypedPresence();
  void ownerReplacementCancelsAndRejectsStaleCaller();
  void authenticatesRequestsAndReturnsScopedPayload();
  void cancelsDelayedRequestAndRefusesUnknownConnection();

private:
  QProcess m_bus;
  QString m_busEndpoint;
  QString m_address;
  QThread m_managerThread;
  std::unique_ptr<FakeAgentManager> m_agentManager;
  std::unique_ptr<FakeSettings> m_settings;
  std::unique_ptr<QDBusConnection> m_manager;
  std::unique_ptr<QDBusConnection> m_agent;
  std::unique_ptr<QDBusConnection> m_presence;
  std::unique_ptr<QDBusConnection> m_foreign;
};

void NetworkSecretAgentDbusTest::initTestCase() {
  registerSecretAgentDBusTypes();
  m_busEndpoint = QStringLiteral("qindaqt-network-secret-agent-%1")
                      .arg(QCoreApplication::applicationPid());
  m_bus.setProgram(QStringLiteral("dbus-daemon"));
  m_bus.setArguments(
      {QStringLiteral("--session"), QStringLiteral("--nofork"),
       QStringLiteral("--nopidfile"),
       QStringLiteral("--address=unix:abstract=%1").arg(m_busEndpoint),
       QStringLiteral("--print-address=1")});
  m_bus.start();
  QVERIFY(m_bus.waitForStarted(5'000));
  QVERIFY(m_bus.waitForReadyRead(5'000));
  m_address = QString::fromUtf8(m_bus.readLine()).trimmed();
  QVERIFY(!m_address.isEmpty());

  const QString suffix = QString::number(QCoreApplication::applicationPid());
  m_manager = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(
      m_address, QStringLiteral("qindaqt-test-nm-") + suffix));
  m_agent = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(
      m_address, QStringLiteral("qindaqt-test-agent-") + suffix));
  m_presence = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(
      m_address, QStringLiteral("qindaqt-test-presence-") + suffix));
  m_foreign = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(
      m_address, QStringLiteral("qindaqt-test-foreign-") + suffix));
  QVERIFY(m_manager->isConnected());
  QVERIFY(m_agent->isConnected());
  QVERIFY(m_presence->isConnected());
  QVERIFY(m_foreign->isConnected());

  m_agentManager = std::make_unique<FakeAgentManager>();
  m_settings = std::make_unique<FakeSettings>();
  m_agentManager->moveToThread(&m_managerThread);
  m_settings->moveToThread(&m_managerThread);
  m_managerThread.start();
  QVERIFY(m_manager->registerObject(QString::fromLatin1(kAgentManagerPath),
                                    m_agentManager.get(),
                                    QDBusConnection::ExportAllSlots));
  QVERIFY(m_manager->registerObject(
      QStringLiteral("/org/freedesktop/NetworkManager/Settings"),
      m_settings.get(), QDBusConnection::ExportAllSlots));
  QVERIFY(
      m_manager->registerService(QString::fromLatin1(kNetworkManagerService)));
}

void NetworkSecretAgentDbusTest::cleanupTestCase() {
  if (m_manager == nullptr) {
    if (m_bus.state() != QProcess::NotRunning) {
      m_bus.kill();
      m_bus.waitForFinished(5'000);
    }
    return;
  }
  m_manager->unregisterService(QString::fromLatin1(kNetworkManagerService));
  m_manager->unregisterObject(QString::fromLatin1(kAgentManagerPath));
  m_manager->unregisterObject(
      QStringLiteral("/org/freedesktop/NetworkManager/Settings"));
  m_managerThread.quit();
  QVERIFY(m_managerThread.wait(5'000));
  m_agentManager.reset();
  m_settings.reset();
  const QStringList names{m_manager->name(), m_agent->name(),
                          m_presence->name(), m_foreign->name()};
  m_manager.reset();
  m_agent.reset();
  m_presence.reset();
  m_foreign.reset();
  for (const QString &name : names) {
    QDBusConnection::disconnectFromBus(name);
  }
  m_bus.terminate();
  if (!m_bus.waitForFinished(5'000)) {
    m_bus.kill();
    QVERIFY(m_bus.waitForFinished(5'000));
  }
}

void NetworkSecretAgentDbusTest::registersAndUnregistersWithTypedPresence() {
  FakePrompt prompt;
  QSignalSpy registered(m_agentManager.get(), &FakeAgentManager::registered);
  QSignalSpy unregistered(m_agentManager.get(),
                          &FakeAgentManager::unregistered);
  ResidentSecretAgent resident(prompt, *m_agent, *m_presence);
  QCOMPARE(resident.start(), ResidentStartStatus::Started);
  QTRY_COMPARE(registered.count(), 1);
  QVERIFY(resident.isRegistered());
  QCOMPARE(registered.first().at(0).toString(),
           QString::fromLatin1(kAgentIdentifier));
  QCOMPARE(registered.first().at(1).toString(), m_agent->baseService());
  const QDBusReply<bool> present = m_foreign->interface()->isServiceRegistered(
      QString::fromLatin1(kPresenceService));
  QVERIFY(present.isValid());
  QVERIFY(present.value());
  resident.stop();
  QTRY_COMPARE(unregistered.count(), 1);
  QVERIFY(!resident.isRegistered());
}

void NetworkSecretAgentDbusTest::
    ownerReplacementCancelsAndRejectsStaleCaller() {
  FakePrompt prompt;
  ResidentSecretAgent resident(prompt, *m_agent, *m_presence);
  QCOMPARE(resident.start(), ResidentStartStatus::Started);
  auto pending =
      watch(*m_manager,
            getSecretsMessage(
                m_agent->baseService(),
                QStringLiteral("/org/freedesktop/NetworkManager/Settings/7")));
  QTRY_COMPARE(prompt.requests.size(), 1);

  QVERIFY(m_manager->unregisterService(
      QString::fromLatin1(kNetworkManagerService)));
  QTRY_VERIFY(pending->isFinished());
  const QDBusPendingReply<NmSettingsMap> canceled = *pending;
  QCOMPARE(canceled.error().name(), QString::fromLatin1(kUserCanceledError));
  QCOMPARE(prompt.canceled.size(), 1);
  QTRY_VERIFY(!resident.isRegistered());

  QVERIFY(m_foreign->registerObject(QString::fromLatin1(kAgentManagerPath),
                                    m_agentManager.get(),
                                    QDBusConnection::ExportAllSlots));
  QVERIFY(m_foreign->registerObject(
      QStringLiteral("/org/freedesktop/NetworkManager/Settings"),
      m_settings.get(), QDBusConnection::ExportAllSlots));
  QVERIFY(
      m_foreign->registerService(QString::fromLatin1(kNetworkManagerService)));
  QTRY_VERIFY(resident.isRegistered());
  QCOMPARE(resident.networkManagerOwner(), m_foreign->baseService());

  auto staleCaller =
      watch(*m_manager,
            getSecretsMessage(
                m_agent->baseService(),
                QStringLiteral("/org/freedesktop/NetworkManager/Settings/7")));
  QTRY_VERIFY(staleCaller->isFinished());
  const QDBusPendingReply<NmSettingsMap> staleReply = *staleCaller;
  QCOMPARE(staleReply.error().name(), QString::fromLatin1(kNoSecretsError));
  resident.stop();
  m_foreign->unregisterService(QString::fromLatin1(kNetworkManagerService));
  m_foreign->unregisterObject(QString::fromLatin1(kAgentManagerPath));
  m_foreign->unregisterObject(
      QStringLiteral("/org/freedesktop/NetworkManager/Settings"));
  QVERIFY(
      m_manager->registerService(QString::fromLatin1(kNetworkManagerService)));
}

void NetworkSecretAgentDbusTest::
    authenticatesRequestsAndReturnsScopedPayload() {
  FakePrompt prompt;
  ResidentSecretAgent resident(prompt, *m_agent, *m_presence);
  QCOMPARE(resident.start(), ResidentStartStatus::Started);

  auto foreign =
      watch(*m_foreign,
            getSecretsMessage(
                m_agent->baseService(),
                QStringLiteral("/org/freedesktop/NetworkManager/Settings/7")));
  QTRY_VERIFY(foreign->isFinished());
  const QDBusPendingReply<NmSettingsMap> foreignReply = *foreign;
  QVERIFY(foreignReply.isError());
  QCOMPARE(foreignReply.error().name(), QString::fromLatin1(kNoSecretsError));
  QVERIFY(prompt.requests.isEmpty());

  auto nonInteractive = watch(
      *m_manager,
      getSecretsMessage(
          m_agent->baseService(),
          QStringLiteral("/org/freedesktop/NetworkManager/Settings/7"), 0U));
  QTRY_VERIFY(nonInteractive->isFinished());
  const QDBusPendingReply<NmSettingsMap> noPromptReply = *nonInteractive;
  QCOMPARE(noPromptReply.error().name(), QString::fromLatin1(kNoSecretsError));

  auto accepted =
      watch(*m_manager,
            getSecretsMessage(
                m_agent->baseService(),
                QStringLiteral("/org/freedesktop/NetworkManager/Settings/7")));
  QTRY_COMPARE(prompt.requests.size(), 1);
  prompt.submit(false);
  QTRY_VERIFY(accepted->isFinished());
  const QDBusPendingReply<NmSettingsMap> reply = *accepted;
  QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
  QCOMPARE(reply.value().keys(),
           QStringList{QStringLiteral("802-11-wireless-security")});
  const QVariantMap section =
      reply.value().value(QStringLiteral("802-11-wireless-security"));
  QCOMPARE(section.value(QStringLiteral("psk")).toString(),
           QStringLiteral("private-bus-canary"));
  QCOMPARE(section.value(QStringLiteral("psk-flags")).toUInt(), 2U);

  auto remembered =
      watch(*m_manager,
            getSecretsMessage(
                m_agent->baseService(),
                QStringLiteral("/org/freedesktop/NetworkManager/Settings/7")));
  QTRY_COMPARE(prompt.requests.size(), 2);
  prompt.submit(true);
  QTRY_VERIFY(remembered->isFinished());
  const QDBusPendingReply<NmSettingsMap> rememberedReply = *remembered;
  QVERIFY(rememberedReply.isValid());
  QCOMPARE(rememberedReply.value()
               .value(QStringLiteral("802-11-wireless-security"))
               .value(QStringLiteral("psk-flags"))
               .toUInt(),
           0U);

  auto save = watch(*m_manager, storageMessage(m_agent->baseService(),
                                               QStringLiteral("SaveSecrets")));
  auto remove =
      watch(*m_manager, storageMessage(m_agent->baseService(),
                                       QStringLiteral("DeleteSecrets")));
  QTRY_VERIFY(save->isFinished());
  QTRY_VERIFY(remove->isFinished());
  QVERIFY(QDBusPendingReply<>(*save).isValid());
  QVERIFY(QDBusPendingReply<>(*remove).isValid());
  auto foreignSave =
      watch(*m_foreign, storageMessage(m_agent->baseService(),
                                       QStringLiteral("SaveSecrets")));
  QTRY_VERIFY(foreignSave->isFinished());
  QCOMPARE(QDBusPendingReply<>(*foreignSave).error().name(),
           QString::fromLatin1(kNoSecretsError));
  resident.stop();
}

void NetworkSecretAgentDbusTest::
    cancelsDelayedRequestAndRefusesUnknownConnection() {
  FakePrompt prompt;
  ResidentSecretAgent resident(prompt, *m_agent, *m_presence);
  QCOMPARE(resident.start(), ResidentStartStatus::Started);

  auto unknown =
      watch(*m_manager,
            getSecretsMessage(
                m_agent->baseService(),
                QStringLiteral("/org/freedesktop/NetworkManager/Settings/99")));
  QTRY_VERIFY(unknown->isFinished());
  const QDBusPendingReply<NmSettingsMap> unknownReply = *unknown;
  QCOMPARE(unknownReply.error().name(), QString::fromLatin1(kNoSecretsError));

  auto pending =
      watch(*m_manager,
            getSecretsMessage(
                m_agent->baseService(),
                QStringLiteral("/org/freedesktop/NetworkManager/Settings/7")));
  QTRY_COMPARE(prompt.requests.size(), 1);
  QDBusMessage cancel = QDBusMessage::createMethodCall(
      m_agent->baseService(), QString::fromLatin1(kSecretAgentPath),
      QString::fromLatin1(kSecretAgentInterface),
      QStringLiteral("CancelGetSecrets"));
  cancel.setArguments({QDBusObjectPath(QStringLiteral(
                           "/org/freedesktop/NetworkManager/Settings/7")),
                       QStringLiteral("802-11-wireless-security")});
  auto cancelReply = watch(*m_manager, cancel);
  QTRY_VERIFY(cancelReply->isFinished());
  QTRY_VERIFY(pending->isFinished());
  const QDBusPendingReply<NmSettingsMap> canceledReply = *pending;
  QCOMPARE(canceledReply.error().name(),
           QString::fromLatin1(kUserCanceledError));
  QCOMPARE(prompt.canceled.size(), 1);
  resident.stop();
}

QTEST_GUILESS_MAIN(NetworkSecretAgentDbusTest)
#include "tst_network_secret_agent_dbus.moc"
