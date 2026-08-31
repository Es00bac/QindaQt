// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_qt_transport/qt_network_transport.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTemporaryDir>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtTest>

#include <csignal>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Client;

namespace {

QString selectedServiceExecutable() {
  const QString override =
      qEnvironmentVariable("QINDAQT_NETWORK_SERVICE_UNDER_TEST");
  return QFileInfo(override.isEmpty()
                       ? QStringLiteral(QINDAQT_NETWORK_SERVICE_EXECUTABLE)
                       : override)
      .canonicalFilePath();
}

bool isExactServiceProcess(const pid_t pid) {
  const QString expected = selectedServiceExecutable();
  return !expected.isEmpty() &&
         QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid))
                 .canonicalFilePath() == expected;
}

void terminateExactServiceProcess(const pid_t pid) {
  if (!isExactServiceProcess(pid)) {
    return;
  }
  ::kill(pid, SIGTERM);
  for (int attempt = 0; attempt < 50 && isExactServiceProcess(pid); ++attempt) {
    QTest::qSleep(20);
  }
  if (isExactServiceProcess(pid)) {
    ::kill(pid, SIGKILL);
  }
}

bool startBareBus(QProcess &daemon, QString *address,
                  const QProcessEnvironment &environment = {}) {
  if (!environment.isEmpty()) {
    daemon.setProcessEnvironment(environment);
  }
  daemon.setProgram(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE));
  daemon.setArguments({QStringLiteral("--session"), QStringLiteral("--nofork"),
                       QStringLiteral("--nopidfile"),
                       QStringLiteral("--print-address=1")});
  daemon.start();
  if (!daemon.waitForStarted() || !daemon.waitForReadyRead(5'000)) {
    return false;
  }
  *address = QString::fromUtf8(daemon.readLine()).trimmed();
  return !address->isEmpty();
}

void stopDaemon(QProcess &daemon) {
  if (daemon.state() == QProcess::NotRunning) {
    return;
  }
  daemon.terminate();
  if (!daemon.waitForFinished(2'000)) {
    daemon.kill();
    daemon.waitForFinished();
  }
}

class PrivateActivatingBuses final {
public:
  bool start(const bool reserveUniqueOwner = false) {
    if (!root.isValid()) {
      return false;
    }
    const QString serviceDir =
        root.filePath(QStringLiteral("share/dbus-1/services"));
    const QString runtimeDir = root.filePath(QStringLiteral("runtime"));
    if (!QDir().mkpath(serviceDir) || !QDir().mkpath(runtimeDir)) {
      return false;
    }
    QFile descriptor(serviceDir +
                     QStringLiteral("/org.qindaqt.Network1.service"));
    if (!descriptor.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return false;
    }
    descriptor.write("[D-BUS Service]\nName=org.qindaqt.Network1\nExec=");
    descriptor.write(selectedServiceExecutable().toUtf8());
    descriptor.write("\n");
    descriptor.close();

    if (!startBareBus(systemDaemon, &systemAddress)) {
      return false;
    }
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_DATA_DIRS"),
                       root.filePath(QStringLiteral("share")));
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtimeDir);
    // AGENT-GUARD: The production libnm adapter is exercised against this
    // empty private bus. No test may observe or mutate the host system bus,
    // NetworkManager owner, radio, device, or connection inventory.
    environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"),
                       systemAddress);
    if (!startBareBus(sessionDaemon, &sessionAddress, environment)) {
      return false;
    }
    connectionName = QStringLiteral("qindaqt-network-activation-%1")
                         .arg(QUuid::createUuid().toString(QUuid::Id128));
    connection = QDBusConnection::connectToBus(sessionAddress, connectionName);
    if (!connection.isConnected()) {
      return false;
    }
    if (reserveUniqueOwner) {
      peerName = connectionName + QStringLiteral("-peer");
      peer = QDBusConnection::connectToBus(sessionAddress, peerName);
      if (!peer.isConnected()) {
        return false;
      }
    }
    return true;
  }

  ~PrivateActivatingBuses() {
    if (servicePid > 0) {
      terminateExactServiceProcess(servicePid);
    }
    if (!connectionName.isEmpty()) {
      QDBusConnection::disconnectFromBus(connectionName);
    }
    if (!peerName.isEmpty()) {
      QDBusConnection::disconnectFromBus(peerName);
    }
    stopDaemon(sessionDaemon);
    stopDaemon(systemDaemon);
  }

  pid_t findServicePid() {
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("GetConnectionUnixProcessID"));
    call.setArguments({QString::fromLatin1(kServiceName)});
    const QDBusMessage reply = connection.call(call);
    if (reply.type() != QDBusMessage::ReplyMessage ||
        reply.arguments().isEmpty()) {
      return 0;
    }
    servicePid = static_cast<pid_t>(reply.arguments().constFirst().toUInt());
    if (!isExactServiceProcess(servicePid)) {
      servicePid = 0;
    }
    return servicePid;
  }

  void stopSessionDaemon() { stopDaemon(sessionDaemon); }

  QTemporaryDir root{QStringLiteral("/tmp/qindaqt-network-activation-XXXXXX")};
  QProcess systemDaemon;
  QProcess sessionDaemon;
  QString systemAddress;
  QString sessionAddress;
  QString connectionName;
  QString peerName;
  QDBusConnection connection{QStringLiteral("invalid")};
  QDBusConnection peer{QStringLiteral("invalid-peer")};
  pid_t servicePid = 0;
};

bool processExists(const pid_t pid) {
  return QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
}

} // namespace

class NetworkActivationTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void activatesAgainstPrivateUnavailableNetworkManager();
  void busLossExitsAndReplacementAdvancesLineage();
};

void NetworkActivationTests::
    activatesAgainstPrivateUnavailableNetworkManager() {
  PrivateActivatingBuses buses;
  QVERIFY(buses.start());
  QtNetworkTransport transport(buses.connection);
  NetworkClient client(transport);
  QVERIFY(client.start());
  QTRY_VERIFY_WITH_TIMEOUT(client.model().snapshot().has_value(), 10'000);
  QTRY_COMPARE_WITH_TIMEOUT(client.model().snapshot()->availability,
                            Availability::Unavailable, 10'000);
  QCOMPARE(client.model().snapshot()->reasonCode,
           QStringLiteral("networkmanager-unavailable"));
  QCOMPARE(client.model().snapshot()->capabilities, Capabilities{});
  QCOMPARE(client.state(), ClientState::Unavailable);
  QVERIFY(client.model().snapshot()->owner.startsWith(QLatin1Char(':')));
  const pid_t pid = buses.findServicePid();
  QVERIFY(pid > 0);
  buses.stopSessionDaemon();
  QTRY_VERIFY_WITH_TIMEOUT(!processExists(pid), 10'000);
  buses.servicePid = 0;
  client.stop();
}

void NetworkActivationTests::busLossExitsAndReplacementAdvancesLineage() {
  PrivateActivatingBuses first;
  QVERIFY(first.start());
  QtNetworkTransport firstTransport(first.connection);
  NetworkClient firstClient(firstTransport);
  QVERIFY(firstClient.start());
  QTRY_VERIFY_WITH_TIMEOUT(firstClient.model().snapshot().has_value(), 10'000);
  QTRY_COMPARE_WITH_TIMEOUT(firstClient.model().snapshot()->availability,
                            Availability::Unavailable, 10'000);
  const QString firstOwner = firstClient.model().snapshot()->owner;
  const quint64 firstEpoch = firstClient.model().snapshot()->epoch;
  const pid_t firstPid = first.findServicePid();
  QVERIFY(firstPid > 0);
  first.stopSessionDaemon();
  QTRY_VERIFY_WITH_TIMEOUT(!processExists(firstPid), 10'000);
  first.servicePid = 0;
  firstClient.stop();

  PrivateActivatingBuses second;
  QVERIFY(second.start(true));
  QtNetworkTransport secondTransport(second.connection);
  NetworkClient secondClient(secondTransport);
  QVERIFY(secondClient.start());
  QTRY_VERIFY_WITH_TIMEOUT(secondClient.model().snapshot().has_value(), 10'000);
  QTRY_COMPARE_WITH_TIMEOUT(secondClient.model().snapshot()->availability,
                            Availability::Unavailable, 10'000);
  const QString secondOwner = secondClient.model().snapshot()->owner;
  const quint64 secondEpoch = secondClient.model().snapshot()->epoch;
  const pid_t secondPid = second.findServicePid();
  QVERIFY(secondPid > 0);
  QVERIFY(secondOwner != firstOwner);
  QVERIFY(secondEpoch > firstEpoch);
  QVERIFY(secondPid != firstPid);
  second.stopSessionDaemon();
  QTRY_VERIFY_WITH_TIMEOUT(!processExists(secondPid), 10'000);
  second.servicePid = 0;
  secondClient.stop();
}

QTEST_GUILESS_MAIN(NetworkActivationTests)
#include "tst_network_activation.moc"
