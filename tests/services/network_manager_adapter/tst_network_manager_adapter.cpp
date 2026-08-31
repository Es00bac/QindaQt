// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_manager_adapter/network_manager_backend.h>
#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_validation.h>

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
  QCOMPARE(fake->calls.size(), 4);
  QCOMPARE(finished.size(), 4);
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

QTEST_GUILESS_MAIN(NetworkManagerAdapterTests)
#include "tst_network_manager_adapter.moc"
