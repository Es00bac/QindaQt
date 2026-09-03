// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_network/network_secret_agent_presence.h>

#include <QtCore/QProcess>
#include <QtDBus/QDBusConnection>
#include <QtTest/QTest>

namespace {

class PrivateBus final {
public:
  bool start() {
    daemon.setProgram(QStringLiteral("dbus-daemon"));
    daemon.setArguments({QStringLiteral("--session"),
                         QStringLiteral("--nofork"),
                         QStringLiteral("--nopidfile"),
                         QStringLiteral("--address=unix:abstract=qindaqt-"
                                        "secret-presence-%1")
                             .arg(QCoreApplication::applicationPid()),
                         QStringLiteral("--print-address=1")});
    daemon.start();
    if (!daemon.waitForStarted(5'000) || !daemon.waitForReadyRead(5'000)) {
      return false;
    }
    address = QString::fromUtf8(daemon.readLine()).trimmed();
    return !address.isEmpty();
  }

  ~PrivateBus() {
    daemon.terminate();
    if (!daemon.waitForFinished(2'000)) {
      daemon.kill();
      daemon.waitForFinished(2'000);
    }
  }

  QProcess daemon;
  QString address;
};

} // namespace

class NetworkSecretAgentPresenceTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void followsOnlyThePublicPresenceName();
};

void NetworkSecretAgentPresenceTest::followsOnlyThePublicPresenceName() {
  PrivateBus bus;
  QVERIFY(bus.start());
  const QString observerName = QStringLiteral("presence-observer");
  const QString ownerName = QStringLiteral("presence-owner");
  QDBusConnection observer =
      QDBusConnection::connectToBus(bus.address, observerName);
  QDBusConnection owner = QDBusConnection::connectToBus(bus.address, ownerName);
  QVERIFY(observer.isConnected());
  QVERIFY(owner.isConnected());
  QindaQt::Apps::SettingsNetwork::NetworkSecretAgentPresence presence(observer);
  QVERIFY(!presence.registered());
  QVERIFY(
      owner.registerService(QStringLiteral("org.qindaqt.NetworkSecretAgent1")));
  QTRY_VERIFY(presence.registered());
  owner.unregisterService(QStringLiteral("org.qindaqt.NetworkSecretAgent1"));
  QTRY_VERIFY(!presence.registered());
  QDBusConnection::disconnectFromBus(ownerName);
  QDBusConnection::disconnectFromBus(observerName);
}

QTEST_GUILESS_MAIN(NetworkSecretAgentPresenceTest)
#include "tst_network_secret_agent_presence.moc"
