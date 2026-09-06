// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_settings_test_support.h"

#include <qindaqt/apps/settings_network/network_settings_model.h>

#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::SettingsNetwork;
using namespace QindaQt::Apps::SettingsNetwork::TestSupport;
using namespace QindaQt::Network;
using namespace QindaQt::Network::Client;

class NetworkSettingsModelTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void projectsBoundedAuthoritativeInventory();
  void dispatchesOnlyAdmittedSecretFreeIntents();
  void blocksActionsDuringAuthoritativeRefreshes();
  void reportsFailedConnectWithoutCredentialEntry();
  void disablesActionsWhenCapabilitiesDisappear();
  void preservesStaleReadOnlyTruthAfterHostileRefresh();
  void limitedCurrentInventoryIsNotStale();
  void clearsOnOwnerLossAndAcceptsFreshReplacement();

private:
  struct Fixture final {
    qint64 now = 1'000;
    FakeNetworkTransport transport;
    NetworkClient client;
    NetworkSettingsModel model;

    Fixture()
        : client(transport, [this] { return now; }, fastTiming()),
          model(client, QDBusConnection(QStringLiteral("no-host-presence"))) {
      transport.setSnapshot(readySnapshot());
      const bool started = client.start();
      Q_ASSERT(started);
      transport.announceOwner(QStringLiteral(":1.20"));
    }
  };
};

void NetworkSettingsModelTest::projectsBoundedAuthoritativeInventory() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);

  QCOMPARE(fixture.model.serviceOwner(), QStringLiteral(":1.20"));
  QCOMPARE(fixture.model.serviceEpoch(), qulonglong(20));
  QCOMPARE(fixture.model.serviceRevision(), qulonglong(1));
  QCOMPARE(fixture.model.connectivityText(),
           QStringLiteral("Connected to the internet"));
  QVERIFY(fixture.model.statusText().contains(QStringLiteral("internet")));
  QVERIFY(fixture.model.scanAvailable());
  QVERIFY(fixture.model.reloadAvailable());
  QVERIFY(!fixture.model.credentialEntrySupported());

  const QVariantList radios = fixture.model.radios();
  QCOMPARE(radios.size(), 2);
  QCOMPARE(radios.at(0).toMap().value(QStringLiteral("name")).toString(),
           QStringLiteral("Wi-Fi"));
  QCOMPARE(
      radios.at(1).toMap().value(QStringLiteral("statusText")).toString(),
      QStringLiteral("Not present"));

  const QVariantList devices = fixture.model.devices();
  QCOMPARE(devices.size(), 2);
  const QVariantMap wifi = devices.at(0).toMap();
  QCOMPARE(wifi.value(QStringLiteral("interfaceName")).toString(),
           QStringLiteral("wlan0"));
  QCOMPARE(wifi.value(QStringLiteral("activeNetworkName")).toString(),
           QStringLiteral("Home"));
  QVERIFY(wifi.value(QStringLiteral("disconnectAvailable")).toBool());

  const QVariantList known = fixture.model.knownNetworks();
  QCOMPARE(known.size(), 2);
  QVERIFY(known.at(0).toMap().value(QStringLiteral("active")).toBool());
  QVERIFY(!known.at(0)
               .toMap()
               .value(QStringLiteral("connectAvailable"))
               .toBool());
  QVERIFY(known.at(0)
              .toMap()
              .value(QStringLiteral("mayRequireExternalCredentials"))
              .toBool());
  QVERIFY(known.at(1)
              .toMap()
              .value(QStringLiteral("connectAvailable"))
              .toBool());

  const QVariantList points = fixture.model.accessPoints();
  QCOMPARE(points.size(), 4);
  for (const QVariant &point : points) {
    QVERIFY(!point.toMap().contains(QStringLiteral("bssid")));
  }
  QCOMPARE(points.at(0).toMap().value(QStringLiteral("signalStrength")).toUInt(),
           quint32(88));
  QVERIFY(points.at(0).toMap().value(QStringLiteral("saved")).toBool());
  QVERIFY(points.at(2)
              .toMap()
              .value(QStringLiteral("connectAvailable"))
              .toBool());
  QVERIFY(!points.at(3)
               .toMap()
               .value(QStringLiteral("connectAvailable"))
               .toBool());
  QVERIFY(points.at(3)
              .toMap()
              .value(QStringLiteral("promptStatusText"))
              .toString()
              .contains(QStringLiteral("no secret agent"),
                        Qt::CaseInsensitive));
}

void NetworkSettingsModelTest::blocksActionsDuringAuthoritativeRefreshes() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);

  fixture.transport.clearSnapshot();
  QVERIFY(fixture.model.requestScan());
  fixture.transport.finishLast(operationResult(
      OperationKind::RequestScan, OperationStatus::Succeeded));
  QTRY_COMPARE_WITH_TIMEOUT(fixture.transport.snapshotRequests.size(), 2,
                            1'000);
  QVERIFY(fixture.model.ready());
  QVERIFY(!fixture.model.busy());
  QVERIFY(!fixture.client.operationAdmissionReady());
  QVERIFY(!fixture.model.scanAvailable());
  QVERIFY(!fixture.model.knownNetworks()
               .at(1)
               .toMap()
               .value(QStringLiteral("connectAvailable"))
               .toBool());
  QVERIFY(!fixture.model.devices()
               .at(0)
               .toMap()
               .value(QStringLiteral("disconnectAvailable"))
               .toBool());
  QVERIFY(!fixture.model.accessPoints()
               .at(2)
               .toMap()
               .value(QStringLiteral("connectAvailable"))
               .toBool());

  const qsizetype operationsBefore = fixture.transport.operations.size();
  QVERIFY(!fixture.model.requestScan());
  QVERIFY(!fixture.model.connectKnownNetwork(networkId(u'b')));
  QVERIFY(!fixture.model.disconnectDevice(QStringLiteral("wlan0")));
  QCOMPARE(fixture.transport.operations.size(), operationsBefore);

  Snapshot refreshed = readySnapshot(QStringLiteral(":1.20"), 20, 2);
  fixture.transport.finishLatestSnapshot(refreshed);
  QTRY_COMPARE_WITH_TIMEOUT(fixture.model.serviceRevision(), qulonglong(2),
                            1'000);
  QVERIFY(fixture.client.operationAdmissionReady());
  QVERIFY(fixture.model.scanAvailable());

  fixture.transport.clearSnapshot();
  fixture.transport.invalidate();
  QVERIFY(!fixture.client.operationAdmissionReady());
  QTRY_COMPARE_WITH_TIMEOUT(fixture.transport.snapshotRequests.size(), 3,
                            1'000);
  QVERIFY(!fixture.model.scanAvailable());
  QVERIFY(!fixture.model.knownNetworks()
               .at(1)
               .toMap()
               .value(QStringLiteral("connectAvailable"))
               .toBool());
  QVERIFY(!fixture.model.devices()
               .at(0)
               .toMap()
               .value(QStringLiteral("disconnectAvailable"))
               .toBool());
  QVERIFY(!fixture.model.accessPoints()
               .at(2)
               .toMap()
               .value(QStringLiteral("connectAvailable"))
               .toBool());

  refreshed.revision = 3;
  fixture.transport.finishLatestSnapshot(refreshed);
  QTRY_COMPARE_WITH_TIMEOUT(fixture.model.serviceRevision(), qulonglong(3),
                            1'000);
  QVERIFY(fixture.client.operationAdmissionReady());
  QVERIFY(fixture.model.scanAvailable());
}

void NetworkSettingsModelTest::dispatchesOnlyAdmittedSecretFreeIntents() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);

  QVERIFY(fixture.model.requestScan());
  QCOMPARE(fixture.transport.operations.size(), 1);
  const auto scan = fixture.transport.operations.constLast();
  QCOMPARE(scan.kind, OperationKind::RequestScan);
  QCOMPARE(scan.owner, QStringLiteral(":1.20"));
  QCOMPARE(scan.epoch, quint64(20));
  QCOMPARE(scan.revision, quint64(1));
  QCOMPARE(scan.parameters.keys(), QStringList{QStringLiteral("deadlineMs")});
  QVERIFY(fixture.model.busy());
  QVERIFY(!fixture.model.scanAvailable());
  QVERIFY(!fixture.model.knownNetworks()
               .at(1)
               .toMap()
               .value(QStringLiteral("connectAvailable"))
               .toBool());

  fixture.transport.finishLast(operationResult(
      OperationKind::RequestScan, OperationStatus::Succeeded));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.client.operationAdmissionReady(), 1'000);
  QVERIFY(fixture.model.operationStatusText().contains(
      QStringLiteral("awaiting fresh results")));

  const QString cafeId = networkId(u'b');
  QVERIFY(fixture.model.connectKnownNetwork(cafeId));
  QCOMPARE(fixture.transport.operations.size(), 2);
  const auto connect = fixture.transport.operations.constLast();
  QCOMPARE(connect.kind, OperationKind::ConnectKnownNetwork);
  QCOMPARE(connect.parameters.keys(),
           QStringList{QStringLiteral("knownNetworkId")});
  QCOMPARE(connect.parameters.value(QStringLiteral("knownNetworkId")).toString(),
           cafeId);
  for (auto it = connect.parameters.cbegin(); it != connect.parameters.cend();
       ++it) {
    QVERIFY(!it.key().contains(QStringLiteral("password"),
                               Qt::CaseInsensitive));
    QVERIFY(!it.key().contains(QStringLiteral("secret"),
                               Qt::CaseInsensitive));
  }
  fixture.transport.finishLast(operationResult(
      OperationKind::ConnectKnownNetwork, OperationStatus::Succeeded));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.client.operationAdmissionReady(), 1'000);

  const QVariantMap guest = fixture.model.accessPoints().at(2).toMap();
  const QString accessPointId = guest.value(QStringLiteral("id")).toString();
  QVERIFY(fixture.model.connectVisibleNetwork(accessPointId));
  QCOMPARE(fixture.transport.operations.size(), 3);
  const auto visible = fixture.transport.operations.constLast();
  QCOMPARE(visible.kind, OperationKind::ConnectVisibleNetwork);
  const QVariantMap expectedVisible{
      {QStringLiteral("accessPointId"), accessPointId}};
  QCOMPARE(visible.parameters, expectedVisible);
  fixture.transport.finishLast(operationResult(
      OperationKind::ConnectVisibleNetwork, OperationStatus::Succeeded));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QTRY_VERIFY_WITH_TIMEOUT(fixture.client.operationAdmissionReady(), 1'000);

  QVERIFY(fixture.model.disconnectDevice(QStringLiteral("wlan0")));
  QCOMPARE(fixture.transport.operations.size(), 4);
  QCOMPARE(fixture.transport.operations.constLast().kind,
           OperationKind::DisconnectActive);
  QCOMPARE(fixture.transport.operations.constLast()
               .parameters.value(QStringLiteral("deviceInterface"))
               .toString(),
           QStringLiteral("wlan0"));
}

void NetworkSettingsModelTest::reportsFailedConnectWithoutCredentialEntry() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);
  const QString cafeId = networkId(u'b');

  QVERIFY(fixture.model.connectKnownNetwork(cafeId));
  fixture.transport.finishLast(operationResult(
      OperationKind::ConnectKnownNetwork, OperationStatus::Failed, 20, 1,
      QStringLiteral("credentials-required")));
  QTRY_VERIFY_WITH_TIMEOUT(!fixture.model.busy(), 1'000);
  QVERIFY(!fixture.model.errorText().contains(
      QStringLiteral("credentials-required")));
  QVERIFY(fixture.model.errorText().contains(
      QStringLiteral("password")));
  QVERIFY(!fixture.model.credentialEntrySupported());

  const qsizetype before = fixture.transport.operations.size();
  QVERIFY(!fixture.model.connectKnownNetwork(QStringLiteral("password=hunter2")));
  QCOMPARE(fixture.transport.operations.size(), before);
  QVERIFY(!fixture.model.errorText().contains(QStringLiteral("hunter2")));
}

void NetworkSettingsModelTest::disablesActionsWhenCapabilitiesDisappear() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);

  Snapshot restricted = readySnapshot(QStringLiteral(":1.20"), 20, 2);
  restricted.capabilities = Capability::Connectivity;
  fixture.transport.setSnapshot(restricted);
  QVERIFY(fixture.model.reload());
  QTRY_COMPARE_WITH_TIMEOUT(fixture.model.serviceRevision(), qulonglong(2),
                            1'000);

  QVERIFY(!fixture.model.scanAvailable());
  QVERIFY(!fixture.model.knownNetworks()
               .at(1)
               .toMap()
               .value(QStringLiteral("connectAvailable"))
               .toBool());
  QVERIFY(!fixture.model.devices()
               .at(0)
               .toMap()
               .value(QStringLiteral("disconnectAvailable"))
               .toBool());
  const QString accessPointId = fixture.model.accessPoints()
                                    .at(2)
                                    .toMap()
                                    .value(QStringLiteral("id"))
                                    .toString();
  QVERIFY(!fixture.model.accessPoints()
               .at(2)
               .toMap()
               .value(QStringLiteral("connectAvailable"))
               .toBool());
  const qsizetype before = fixture.transport.operations.size();
  QVERIFY(!fixture.model.requestScan());
  QVERIFY(!fixture.model.connectKnownNetwork(networkId(u'b')));
  QVERIFY(!fixture.model.connectVisibleNetwork(accessPointId));
  QVERIFY(!fixture.model.disconnectDevice(QStringLiteral("wlan0")));
  QCOMPARE(fixture.transport.operations.size(), before);
}

void NetworkSettingsModelTest::limitedCurrentInventoryIsNotStale() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);
  Snapshot limited = readySnapshot();
  ++limited.revision;
  limited.availability = Availability::Degraded;
  limited.reasonCode = QStringLiteral("networkmanager-data-degraded");
  fixture.transport.setSnapshot(limited);
  QVERIFY(fixture.model.reload());
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.degraded(), 1'000);
  QVERIFY(fixture.client.snapshotCurrent());
  QVERIFY(!fixture.model.stale());
  QVERIFY(!fixture.model.statusText().contains(QStringLiteral("stale")));
}

void NetworkSettingsModelTest::preservesStaleReadOnlyTruthAfterHostileRefresh() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);
  const QVariantList before = fixture.model.knownNetworks();

  fixture.transport.setPayload(QByteArrayLiteral("not-a-network-snapshot"));
  QVERIFY(fixture.model.reload());
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.degraded(), 1'000);
  QVERIFY(fixture.model.stale());
  QCOMPARE(fixture.model.knownNetworks().size(), before.size());
  QCOMPARE(fixture.model.serviceOwner(), QStringLiteral(":1.20"));
  QVERIFY(!fixture.model.scanAvailable());
  QVERIFY(!fixture.model.knownNetworks()
               .at(1)
               .toMap()
               .value(QStringLiteral("connectAvailable"))
               .toBool());
}

void NetworkSettingsModelTest::clearsOnOwnerLossAndAcceptsFreshReplacement() {
  Fixture fixture;
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);
  QVERIFY(fixture.model.connectKnownNetwork(networkId(u'b')));
  const auto retiredCall = fixture.transport.operations.constLast();

  fixture.transport.announceOwner(QString());
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.unavailable(), 1'000);
  QVERIFY(!fixture.model.stale());
  QVERIFY(fixture.model.serviceOwner().isEmpty());
  QVERIFY(fixture.model.devices().isEmpty());
  QVERIFY(fixture.model.knownNetworks().isEmpty());
  QVERIFY(fixture.model.errorText().contains(QStringLiteral("uncertain")));

  // A late operation reply from the retired owner is not presentation truth.
  const auto late = operationResult(OperationKind::ConnectKnownNetwork,
                                    OperationStatus::Succeeded);
  const EncodeResult encoded = encodeOperationResult(late);
  QVERIFY(encoded.succeeded());
  Q_EMIT fixture.transport.operationReceived(retiredCall.token,
                                              retiredCall.owner,
                                              encoded.payload);
  QVERIFY(fixture.model.knownNetworks().isEmpty());

  fixture.transport.setSnapshot(
      readySnapshot(QStringLiteral(":1.21"), 21, 1));
  fixture.transport.announceOwner(QStringLiteral(":1.21"));
  QTRY_VERIFY_WITH_TIMEOUT(fixture.model.ready(), 1'000);
  QCOMPARE(fixture.model.serviceOwner(), QStringLiteral(":1.21"));
  QCOMPARE(fixture.model.serviceEpoch(), qulonglong(21));
  QCOMPARE(fixture.model.serviceRevision(), qulonglong(1));
  QCOMPARE(fixture.model.knownNetworks().size(), 2);
}

QTEST_MAIN(NetworkSettingsModelTest)
#include "tst_network_settings_model.moc"
