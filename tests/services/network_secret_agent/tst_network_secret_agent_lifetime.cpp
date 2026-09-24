// SPDX-License-Identifier: GPL-3.0-or-later

// Process-level lifetime regression for the built qindaqt-network-secret-agent
// executable. The real QML prompt closes through the same QWindow::close()
// path as Escape, the window close button, and CancelGetSecrets, so closing
// the only prompt must leave the resident agent running and registered.

#include "private_network_manager_bus.h"

#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusPendingReply>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Network::SecretAgent;
using namespace QindaQt::Network::SecretAgent::Testing;

class NetworkSecretAgentLifetimeTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void staysRegisteredAfterLastPromptClosesAndPromptsAgain();

private:
  // Sends GetSecrets, proves the prompt is held open, then closes it with
  // CancelGetSecrets and expects the standard UserCanceled completion.
  void promptAndClose(const QString &agentService);

  PrivateNetworkManagerBus m_bus;
  QProcess m_process;
};

void NetworkSecretAgentLifetimeTest::initTestCase() {
  const QString failure = m_bus.start();
  QVERIFY2(failure.isEmpty(), qPrintable(failure));
}

void NetworkSecretAgentLifetimeTest::cleanupTestCase() {
  if (m_process.state() != QProcess::NotRunning) {
    m_process.kill();
    m_process.waitForFinished(5'000);
  }
  const QString failure = m_bus.stop();
  QVERIFY2(failure.isEmpty(), qPrintable(failure));
}

void NetworkSecretAgentLifetimeTest::promptAndClose(
    const QString &agentService) {
  const QString path = QString::fromLatin1(kKnownConnectionPath);
  auto pending = watch(m_bus.manager(), getSecretsMessage(agentService, path));
  // A refused request completes at once with NoSecrets; a shown prompt holds
  // the delayed reply open until it is closed.
  QTest::qWait(500);
  QVERIFY2(
      !pending->isFinished(),
      qPrintable(QDBusPendingReply<NmSettingsMap>(*pending).error().name()));
  auto cancel =
      watch(m_bus.manager(), cancelGetSecretsMessage(agentService, path));
  QTRY_VERIFY(cancel->isFinished());
  QTRY_VERIFY(pending->isFinished());
  QCOMPARE(QDBusPendingReply<NmSettingsMap>(*pending).error().name(),
           QString::fromLatin1(kUserCanceledError));
}

void NetworkSecretAgentLifetimeTest::
    staysRegisteredAfterLastPromptClosesAndPromptsAgain() {
  QSignalSpy registered(m_bus.agentManager(), &FakeAgentManager::registered);
  QSignalSpy unregistered(m_bus.agentManager(),
                          &FakeAgentManager::unregistered);

  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"),
                     m_bus.address());
  environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                     m_bus.address());
  environment.insert(QStringLiteral("QT_QPA_PLATFORM"),
                     QStringLiteral("offscreen"));
  environment.insert(QStringLiteral("QT_QUICK_BACKEND"),
                     QStringLiteral("software"));
  environment.remove(QStringLiteral("DISPLAY"));
  environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
  m_process.setProcessEnvironment(environment);
  m_process.setProgram(QStringLiteral(QINDAQT_NETWORK_SECRET_AGENT_EXECUTABLE));
  m_process.start();
  QVERIFY(m_process.waitForStarted(5'000));
  QTRY_COMPARE_WITH_TIMEOUT(registered.count(), 1, 15'000);
  const QString agentService = registered.first().at(1).toString();
  QVERIFY(!agentService.isEmpty());

  promptAndClose(agentService);

  // AGENT-GUARD: With Qt's default quitOnLastWindowClosed the process quits
  // here, unregisters, and NetworkManager finds no agent for the next join.
  QTest::qWait(1'000);
  QCOMPARE(m_process.state(), QProcess::Running);
  QCOMPARE(unregistered.count(), 0);

  promptAndClose(agentService);
  QCOMPARE(m_process.state(), QProcess::Running);
  QCOMPARE(unregistered.count(), 0);
  QCOMPARE(registered.count(), 1);

  m_process.terminate();
  if (!m_process.waitForFinished(5'000)) {
    m_process.kill();
    QVERIFY(m_process.waitForFinished(5'000));
  }
  const QByteArray diagnostics =
      m_process.readAllStandardOutput() + m_process.readAllStandardError();
  QVERIFY2(!diagnostics.contains("startup failed"), diagnostics.constData());
}

QTEST_GUILESS_MAIN(NetworkSecretAgentLifetimeTest)
#include "tst_network_secret_agent_lifetime.moc"
