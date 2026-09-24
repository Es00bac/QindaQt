// SPDX-License-Identifier: GPL-3.0-or-later

#include "private_network_manager_bus.h"

#include <qindaqt/services/network_secret_agent/resident_secret_agent.h>
#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusReply>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Network::SecretAgent;
using namespace QindaQt::Network::SecretAgent::Testing;

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
  PrivateNetworkManagerBus m_bus;
  FakeAgentManager *m_agentManager = nullptr;
  FakeSettings *m_settings = nullptr;
  QDBusConnection *m_manager = nullptr;
  QDBusConnection *m_agent = nullptr;
  QDBusConnection *m_presence = nullptr;
  QDBusConnection *m_foreign = nullptr;
};

void NetworkSecretAgentDbusTest::initTestCase() {
  const QString failure = m_bus.start();
  QVERIFY2(failure.isEmpty(), qPrintable(failure));
  m_agentManager = m_bus.agentManager();
  m_settings = m_bus.settings();
  m_manager = &m_bus.manager();
  m_agent = &m_bus.agent();
  m_presence = &m_bus.presence();
  m_foreign = &m_bus.foreign();
}

void NetworkSecretAgentDbusTest::cleanupTestCase() {
  const QString failure = m_bus.stop();
  QVERIFY2(failure.isEmpty(), qPrintable(failure));
}

void NetworkSecretAgentDbusTest::registersAndUnregistersWithTypedPresence() {
  FakePrompt prompt;
  QSignalSpy registered(m_agentManager, &FakeAgentManager::registered);
  QSignalSpy unregistered(m_agentManager, &FakeAgentManager::unregistered);
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
                                    m_agentManager,
                                    QDBusConnection::ExportAllSlots));
  QVERIFY(m_foreign->registerObject(
      QStringLiteral("/org/freedesktop/NetworkManager/Settings"), m_settings,
      QDBusConnection::ExportAllSlots));
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
  const QDBusMessage cancel = cancelGetSecretsMessage(
      m_agent->baseService(),
      QStringLiteral("/org/freedesktop/NetworkManager/Settings/7"));
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
