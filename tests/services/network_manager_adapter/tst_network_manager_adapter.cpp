// SPDX-License-Identifier: GPL-3.0-or-later

#include "libnm_network_manager_port_p.h"

#include <qindaqt/services/network_manager_adapter/network_manager_backend.h>
#include <qindaqt/services/network_model/network_model.h>
#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_validation.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusReply>
#include <QtTest>

#include <algorithm>
#include <optional>

using namespace QindaQt::Network;
using namespace QindaQt::Network::NetworkManager;
using namespace QindaQt::Network::Service;

namespace {

class FakeNetworkManagerPort final : public NetworkManagerPort {
  Q_OBJECT

public:
  struct Call final {
    quint64 id = 0;
    BackendOperationRequest request;
  };

  bool start() override {
    ++startCalls;
    running = startResult;
    if (startFacts.has_value()) {
      Q_EMIT factsReady(*startFacts);
    }
    return startResult;
  }
  void stop() override {
    ++stopCalls;
    running = false;
  }
  void submit(const quint64 operationId,
              const BackendOperationRequest &request) override {
    calls.append({operationId, request});
  }
  void cancel(const quint64 operationId) override {
    cancelled.append(operationId);
  }
  void publish(const Facts &facts) { Q_EMIT factsReady(facts); }
  void finish(const quint64 id, const BackendOperationOutcome &outcome) {
    Q_EMIT operationFinished(id, outcome);
  }
  void replace() { Q_EMIT authorityReplaced(); }

  QList<Call> calls;
  QList<quint64> cancelled;
  int startCalls = 0;
  int stopCalls = 0;
  bool startResult = true;
  bool running = false;
  std::optional<Facts> startFacts;
};

class PrivateSystemBus final {
public:
  bool start() {
    daemon.setProgram(QStringLiteral("dbus-daemon"));
    daemon.setArguments(
        {QStringLiteral("--session"), QStringLiteral("--nofork"),
         QStringLiteral("--nopidfile"), QStringLiteral("--print-address=1")});
    daemon.start();
    if (!daemon.waitForStarted() || !daemon.waitForReadyRead(5'000)) {
      return false;
    }
    address = QString::fromUtf8(daemon.readLine()).trimmed();
    return !address.isEmpty();
  }

  ~PrivateSystemBus() {
    daemon.terminate();
    if (!daemon.waitForFinished(1'000)) {
      daemon.kill();
      daemon.waitForFinished();
    }
  }

  QProcess daemon;
  QString address;
};

class ScopedEnvironment final {
public:
  ScopedEnvironment(const char *name, const QByteArray &value)
      : m_name(name), m_wasSet(qEnvironmentVariableIsSet(name)),
        m_original(qgetenv(name)) {
    qputenv(name, value);
  }
  ~ScopedEnvironment() {
    if (m_wasSet) {
      qputenv(m_name.constData(), m_original);
    } else {
      qunsetenv(m_name.constData());
    }
  }

private:
  QByteArray m_name;
  bool m_wasSet = false;
  QByteArray m_original;
};

QDBusConnectionInterface::RegisterServiceReply registerNetworkManager(
    const QDBusConnection &connection,
    const QDBusConnectionInterface::ServiceQueueOptions queueOption) {
  const QDBusReply<QDBusConnectionInterface::RegisterServiceReply> reply =
      connection.interface()->registerService(
          QStringLiteral("org.freedesktop.NetworkManager"), queueOption,
          QDBusConnectionInterface::AllowReplacement);
  return reply.isValid() ? reply.value()
                         : QDBusConnectionInterface::ServiceNotRegistered;
}

Facts validFacts() {
  Facts facts;
  facts.available = true;
  facts.connectivity = ConnectivityKind::Full;
  facts.radios = {{RadioKind::Wifi, true, true, true}};
  facts.devices = {
      {QStringLiteral("wlan0"), DeviceKind::Wifi, DeviceState::Connected},
      {QStringLiteral("enp3s0"), DeviceKind::Ethernet, DeviceState::Connected}};
  facts.accessPoints = {{QStringLiteral("wlan0"), QByteArray("Cafe"),
                         QStringLiteral("02:AA:BB:CC:DD:EE"),
                         SecuritySuite::Wpa2Personal, 5'180, 71},
                        {QStringLiteral("wlan0"), QByteArray("\xff\xfe", 2),
                         QStringLiteral("02:00:00:00:00:01"),
                         SecuritySuite::Open, 2'412, 20}};
  facts.knownNetworks = {
      {QByteArray("Cafe"), SecuritySuite::Wpa2Personal, true}};
  facts.activeConnections = {{QStringLiteral("wlan0"), QByteArray("Cafe"),
                              SecuritySuite::Wpa2Personal}};
  facts.scanSupported = true;
  facts.knownNetworkControlSupported = true;
  facts.visibleNetworkControlSupported = true;
  facts.radioControlSupported = true;
  facts.disconnectSupported = true;
  return facts;
}

} // namespace

class NetworkManagerAdapterTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void defersSynchronousStartFactsUntilGenerationIsReturned();
  void mapsBoundedSecretFreeFacts();
  void degradesMalformedAndUnavailableFacts();
  void dispatchesEveryPermittedIntentThroughInjectedPort();
  void fencesStopAndAuthorityReplacement();
  void concreteOwnerNotificationFencesLossAndReplacementBelowPoll();
  void definiteScanFailureClearsLeaseAndAllowsImmediateRetry();
};

void NetworkManagerAdapterTests::
    defersSynchronousStartFactsUntilGenerationIsReturned() {
  auto port = std::make_unique<FakeNetworkManagerPort>();
  port->startFacts = validFacts();
  NetworkManagerBackend backend(std::move(port));
  QSignalSpy observed(&backend, &NetworkBackend::observationReady);
  const quint64 generation = backend.start();
  QVERIFY(generation != 0);
  QCOMPARE(observed.size(), 0);
  QTRY_COMPARE_WITH_TIMEOUT(observed.size(), 1, 2'000);
  QCOMPARE(observed.first().at(0).toULongLong(), generation);
}

void NetworkManagerAdapterTests::mapsBoundedSecretFreeFacts() {
  auto port = std::make_unique<FakeNetworkManagerPort>();
  FakeNetworkManagerPort *fake = port.get();
  NetworkManagerBackend backend(std::move(port));
  QSignalSpy observed(&backend, &NetworkBackend::observationReady);
  const quint64 generation = backend.start();
  QVERIFY(generation != 0);
  fake->publish(validFacts());
  QCOMPARE(observed.size(), 1);
  const BackendObservation result =
      observed.first().at(1).value<BackendObservation>();
  QCOMPARE(result.availability, Availability::Ready);
  QVERIFY(result.capabilities.testFlag(Capability::VisibleNetworkControl));
  QCOMPARE(result.devices.first().interfaceName, QStringLiteral("enp3s0"));
  const auto cafePoint =
      std::find_if(result.accessPoints.cbegin(), result.accessPoints.cend(),
                   [](const AccessPoint &point) {
                     return point.bssid == QStringLiteral("02:aa:bb:cc:dd:ee");
                   });
  QVERIFY(cafePoint != result.accessPoints.cend());
  const auto hiddenPoint =
      std::find_if(result.accessPoints.cbegin(), result.accessPoints.cend(),
                   [](const AccessPoint &point) { return point.hidden; });
  QVERIFY(hiddenPoint != result.accessPoints.cend());
  QVERIFY(hiddenPoint->ssid.isEmpty());
  const QString expectedId =
      knownNetworkId(QByteArray("Cafe"), SecuritySuite::Wpa2Personal);
  QCOMPARE(result.knownNetworks.first().id, expectedId);
  QCOMPARE(result.activeConnections.first().knownNetworkId, expectedId);

  Snapshot snapshot;
  snapshot.owner = QStringLiteral(":1.70");
  snapshot.epoch = 70;
  snapshot.revision = 1;
  snapshot.availability = result.availability;
  snapshot.capabilities = result.capabilities;
  snapshot.connectivity = result.connectivity;
  snapshot.radios = result.radios;
  snapshot.devices = result.devices;
  snapshot.accessPoints = result.accessPoints;
  snapshot.knownNetworks = result.knownNetworks;
  snapshot.activeConnections = result.activeConnections;
  QVERIFY(validateSnapshot(snapshot).accepted);
}

void NetworkManagerAdapterTests::degradesMalformedAndUnavailableFacts() {
  auto port = std::make_unique<FakeNetworkManagerPort>();
  FakeNetworkManagerPort *fake = port.get();
  NetworkManagerBackend backend(std::move(port));
  QSignalSpy observed(&backend, &NetworkBackend::observationReady);
  QVERIFY(backend.start() != 0);
  Facts malformed = validFacts();
  malformed.devices.first().interfaceName = QStringLiteral("bad/interface");
  malformed.accessPoints.first().rawSsid = QByteArray(33, 'x');
  fake->publish(malformed);
  const BackendObservation degraded =
      observed.takeFirst().at(1).value<BackendObservation>();
  QCOMPARE(degraded.availability, Availability::Degraded);
  QCOMPARE(degraded.reasonCode, QStringLiteral("networkmanager-data-degraded"));
  QVERIFY(degraded.activeConnections.isEmpty());

  Facts unavailable;
  fake->publish(unavailable);
  const BackendObservation absent =
      observed.takeFirst().at(1).value<BackendObservation>();
  QCOMPARE(absent.availability, Availability::Unavailable);
  QCOMPARE(absent.reasonCode, QStringLiteral("networkmanager-unavailable"));
  QCOMPARE(absent.capabilities, Capabilities{});
}

void NetworkManagerAdapterTests::
    dispatchesEveryPermittedIntentThroughInjectedPort() {
  auto port = std::make_unique<FakeNetworkManagerPort>();
  FakeNetworkManagerPort *fake = port.get();
  NetworkManagerBackend backend(std::move(port));
  QSignalSpy finished(&backend, &NetworkBackend::operationFinished);
  QVERIFY(backend.start() != 0);

  QList<BackendOperationRequest> requests;
  BackendOperationRequest scan;
  scan.kind = OperationKind::RequestScan;
  scan.scanDeadlineMilliseconds = 30'000;
  requests.append(scan);
  BackendOperationRequest connect;
  connect.kind = OperationKind::ConnectKnownNetwork;
  connect.identifier =
      knownNetworkId(QByteArray("Cafe"), SecuritySuite::Wpa2Personal);
  requests.append(connect);
  BackendOperationRequest visible;
  visible.kind = OperationKind::ConnectVisibleNetwork;
  visible.identifier = visibleAccessPointId(
      QStringLiteral("wlan0"), QStringLiteral("02:aa:bb:cc:dd:ee"));
  requests.append(visible);
  BackendOperationRequest disconnect;
  disconnect.kind = OperationKind::DisconnectActive;
  disconnect.identifier = QStringLiteral("wlan0");
  requests.append(disconnect);
  BackendOperationRequest radio;
  radio.kind = OperationKind::SetRadio;
  radio.radioKind = RadioKind::Wifi;
  radio.enable = false;
  requests.append(radio);

  quint64 id = 1;
  for (const BackendOperationRequest &request : std::as_const(requests)) {
    backend.submit(id, request);
    QCOMPARE(fake->calls.constLast().request, request);
    BackendOperationOutcome success;
    success.status = BackendOperationStatus::Succeeded;
    fake->finish(id, success);
    ++id;
  }
  QCOMPARE(fake->calls.size(), 5);
  QCOMPARE(finished.size(), 5);
  backend.cancel(99);
  QCOMPARE(fake->cancelled, QList<quint64>{99});
}

void NetworkManagerAdapterTests::fencesStopAndAuthorityReplacement() {
  auto port = std::make_unique<FakeNetworkManagerPort>();
  FakeNetworkManagerPort *fake = port.get();
  NetworkManagerBackend backend(std::move(port));
  QSignalSpy observed(&backend, &NetworkBackend::observationReady);
  QSignalSpy replaced(&backend, &NetworkBackend::authorityReplaced);
  QVERIFY(backend.start() != 0);
  fake->replace();
  QCOMPARE(replaced.size(), 1);
  backend.stop();
  fake->publish(validFacts());
  QCOMPARE(observed.size(), 0);
  BackendOperationOutcome late;
  late.status = BackendOperationStatus::Succeeded;
  fake->finish(1, late);
}

void NetworkManagerAdapterTests::
    concreteOwnerNotificationFencesLossAndReplacementBelowPoll() {
  PrivateSystemBus bus;
  QVERIFY(bus.start());
  ScopedEnvironment systemBus("DBUS_SYSTEM_BUS_ADDRESS", bus.address.toUtf8());
  const QString suffix = QUuid::createUuid().toString(QUuid::Id128);
  const QString aName =
      QStringLiteral("qindaqt-network-owner-a-%1").arg(suffix);
  const QString bName =
      QStringLiteral("qindaqt-network-owner-b-%1").arg(suffix);
  QDBusConnection ownerA = QDBusConnection::connectToBus(bus.address, aName);
  QDBusConnection ownerB = QDBusConnection::connectToBus(bus.address, bName);
  QVERIFY(ownerA.isConnected());
  QVERIFY(ownerB.isConnected());
  QCOMPARE(registerNetworkManager(ownerA,
                                  QDBusConnectionInterface::DontQueueService),
           QDBusConnectionInterface::ServiceRegistered);

  LibnmNetworkManagerPort port;
  QSignalSpy facts(&port, &NetworkManagerPort::factsReady);
  QSignalSpy replaced(&port, &NetworkManagerPort::authorityReplaced);
  QVERIFY(port.start());
  QTRY_VERIFY_WITH_TIMEOUT(!facts.isEmpty(), 2'000);

  QElapsedTimer boundary;
  boundary.start();
  QVERIFY(ownerA.unregisterService(
      QStringLiteral("org.freedesktop.NetworkManager")));
  QTRY_COMPARE_WITH_TIMEOUT(replaced.size(), 1, 500);
  QVERIFY(boundary.elapsed() < 750);
  port.stop();

  QCOMPARE(registerNetworkManager(ownerA,
                                  QDBusConnectionInterface::DontQueueService),
           QDBusConnectionInterface::ServiceRegistered);
  QTest::qWait(100);
  QCOMPARE(replaced.size(), 1);
  replaced.clear();
  facts.clear();
  QVERIFY(port.start());
  QTRY_VERIFY_WITH_TIMEOUT(!facts.isEmpty(), 2'000);
  boundary.restart();
  QCOMPARE(registerNetworkManager(
               ownerB, QDBusConnectionInterface::ReplaceExistingService),
           QDBusConnectionInterface::ServiceRegistered);
  QTRY_COMPARE_WITH_TIMEOUT(replaced.size(), 1, 500);
  QVERIFY(boundary.elapsed() < 750);
  port.stop();

  replaced.clear();
  facts.clear();
  QVERIFY(port.start());
  QTRY_VERIFY_WITH_TIMEOUT(!facts.isEmpty(), 2'000);
  boundary.restart();
  QCOMPARE(registerNetworkManager(
               ownerA, QDBusConnectionInterface::ReplaceExistingService),
           QDBusConnectionInterface::ServiceRegistered);
  QTRY_COMPARE_WITH_TIMEOUT(replaced.size(), 1, 500);
  QVERIFY(boundary.elapsed() < 750);
  port.stop();

  QDBusConnection::disconnectFromBus(aName);
  QDBusConnection::disconnectFromBus(bName);
}

void NetworkManagerAdapterTests::
    definiteScanFailureClearsLeaseAndAllowsImmediateRetry() {
  constexpr qint64 now = 1'000;
  ScanLeaseState lease;
  lease.begin(now + 30'000);
  Facts scanning;
  lease.applyTo(scanning, now);
  QCOMPARE(scanning.scanPhase, ScanPhase::Scanning);

  lease.complete(false, false);
  Facts failed;
  lease.applyTo(failed, now);
  QCOMPARE(failed.scanPhase, ScanPhase::Idle);
  QCOMPARE(failed.scanLeaseRemainingMilliseconds, qint64(0));

  Model::NetworkModel model;
  Snapshot snapshot;
  snapshot.owner = QStringLiteral(":1.80");
  snapshot.epoch = 80;
  snapshot.revision = 1;
  snapshot.availability = Availability::Ready;
  snapshot.capabilities = Capability::Scan;
  snapshot.scanPhase = failed.scanPhase;
  QVERIFY(model.applySnapshot(snapshot).accepted);
  QVERIFY(model.requestScan(RequestScanIntent{30'000}).allowed);

  lease.begin(now + 30'000);
  lease.complete(false, true);
  Facts cancelled;
  lease.applyTo(cancelled, now);
  QCOMPARE(cancelled.scanPhase, ScanPhase::Leased);
  QCOMPARE(cancelled.scanLeaseRemainingMilliseconds, qint64(30'000));
}

QTEST_GUILESS_MAIN(NetworkManagerAdapterTests)
#include "tst_network_manager_adapter.moc"
