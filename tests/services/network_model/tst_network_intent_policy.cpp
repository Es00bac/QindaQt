// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_protocol_test_data.h"

#include <qindaqt/services/network_model/network_intent_policy.h>
#include <qindaqt/services/network_model/network_scan_lease.h>

#include <QtTest>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Model;
using namespace QindaQt::Network::TestData;

class NetworkIntentPolicyTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void rejectsIntentsWithoutSnapshotOrReadiness();
  void rejectsUnsupportedCapabilities();
  void validatesScanIntents();
  void rejectsScanWhileBusyOrLeased();
  void validatesConnectIntents();
  void validatesDisconnectIntents();
  void validatesSetRadioIntents();

private:
  static MonotonicClock at(qint64 milliseconds) {
    return MonotonicClock([milliseconds] { return milliseconds; });
  }
};

void NetworkIntentPolicyTests::rejectsIntentsWithoutSnapshotOrReadiness() {
  const ScanLeaseTracker lease;
  QVERIFY(!validateRequestScan(std::nullopt, lease, at(0),
                               RequestScanIntent{30'000})
               .allowed);
  QVERIFY(!validateConnect(std::nullopt, ConnectIntent{cafeNetwork().id}).allowed);
  QVERIFY(
      !validateDisconnect(std::nullopt, DisconnectIntent{QStringLiteral("wlan0")})
           .allowed);
  QVERIFY(!validateSetRadio(std::nullopt, SetRadioIntent{RadioKind::Wifi, true})
               .allowed);

  Snapshot unavailable = validSnapshot();
  unavailable.availability = Availability::Unavailable;
  unavailable.reasonCode = QStringLiteral("upstream-absent");
  QVERIFY(!validateConnect(unavailable, ConnectIntent{cafeNetwork().id}).allowed);
}

void NetworkIntentPolicyTests::rejectsUnsupportedCapabilities() {
  Snapshot withoutScan = validSnapshot();
  withoutScan.capabilities &= ~Capabilities(Capability::Scan);
  const ScanLeaseTracker lease;
  const IntentVerdict scan =
      validateRequestScan(withoutScan, lease, at(0), RequestScanIntent{30'000});
  QVERIFY(!scan.allowed);
  QCOMPARE(scan.reasonCode, QStringLiteral("scan-unsupported"));

  Snapshot withoutKnown = validSnapshot();
  withoutKnown.capabilities &= ~Capabilities(Capability::KnownNetworkControl);
  const IntentVerdict connect =
      validateConnect(withoutKnown, ConnectIntent{cafeNetwork().id});
  QVERIFY(!connect.allowed);
  QCOMPARE(connect.reasonCode,
           QStringLiteral("known-network-control-unsupported"));

  Snapshot withoutActive = validSnapshot();
  withoutActive.capabilities &= ~Capabilities(Capability::ActiveConnectionControl);
  const IntentVerdict disconnect = validateDisconnect(
      withoutActive, DisconnectIntent{QStringLiteral("wlan0")});
  QVERIFY(!disconnect.allowed);

  Snapshot withoutRadio = validSnapshot();
  withoutRadio.capabilities &= ~Capabilities(Capability::RadioControl);
  QVERIFY(!validateSetRadio(withoutRadio,
                            SetRadioIntent{RadioKind::Wifi, false})
               .allowed);
}

void NetworkIntentPolicyTests::validatesScanIntents() {
  const Snapshot snapshot = validSnapshot();
  const ScanLeaseTracker lease;
  QVERIFY(validateRequestScan(snapshot, lease, at(0), RequestScanIntent{30'000})
              .allowed);
  QVERIFY(validateRequestScan(snapshot, lease, at(0),
                              RequestScanIntent{kMinimumScanDeadlineMilliseconds})
              .allowed);
  QVERIFY(validateRequestScan(snapshot, lease, at(0),
                              RequestScanIntent{kMaximumScanDeadlineMilliseconds})
              .allowed);
  QVERIFY(!validateRequestScan(snapshot, lease, at(0),
                               RequestScanIntent{kMinimumScanDeadlineMilliseconds - 1})
               .allowed);
  QVERIFY(!validateRequestScan(snapshot, lease, at(0),
                               RequestScanIntent{kMaximumScanDeadlineMilliseconds + 1})
               .allowed);
}

void NetworkIntentPolicyTests::rejectsScanWhileBusyOrLeased() {
  const ScanLeaseTracker lease;
  Snapshot scanning = leasedSnapshot();
  scanning.scanPhase = ScanPhase::Scanning;
  const IntentVerdict busy =
      validateRequestScan(scanning, lease, at(0), RequestScanIntent{30'000});
  QVERIFY(!busy.allowed);
  QCOMPARE(busy.reasonCode, QStringLiteral("scan-busy"));

  ScanLeaseTracker held;
  held.adopt(ScanLease{QStringLiteral("lease-1"), 41, 7, 500'000}, 41);
  Snapshot leased = leasedSnapshot();
  const IntentVerdict heldVerdict =
      validateRequestScan(leased, held, at(1'000), RequestScanIntent{30'000});
  QVERIFY(!heldVerdict.allowed);
  QCOMPARE(heldVerdict.reasonCode, QStringLiteral("scan-lease-held"));

  // An expired lease no longer pins the scan result set.
  const IntentVerdict afterExpiry =
      validateRequestScan(leased, held, at(600'000), RequestScanIntent{30'000});
  QVERIFY2(afterExpiry.allowed, qPrintable(afterExpiry.reasonCode));
}

void NetworkIntentPolicyTests::validatesConnectIntents() {
  const Snapshot snapshot = validSnapshot();
  QVERIFY(validateConnect(snapshot, ConnectIntent{cafeNetwork().id}).allowed);

  const IntentVerdict unknown =
      validateConnect(snapshot, ConnectIntent{QStringLiteral("deadbeef")});
  QVERIFY(!unknown.allowed);
  QCOMPARE(unknown.reasonCode, QStringLiteral("unknown-known-network"));

  Snapshot active = validSnapshot();
  active.activeConnections = {
      ActiveConnection{QStringLiteral("wlan0"), cafeNetwork().id}};
  const IntentVerdict duplicate =
      validateConnect(active, ConnectIntent{cafeNetwork().id});
  QVERIFY(!duplicate.allowed);
  QCOMPARE(duplicate.reasonCode, QStringLiteral("network-already-active"));
}

void NetworkIntentPolicyTests::validatesDisconnectIntents() {
  Snapshot snapshot = validSnapshot();
  snapshot.activeConnections = {
      ActiveConnection{QStringLiteral("wlan0"), cafeNetwork().id}};
  QVERIFY(validateDisconnect(snapshot, DisconnectIntent{QStringLiteral("wlan0")})
              .allowed);

  const IntentVerdict unknownDevice =
      validateDisconnect(snapshot, DisconnectIntent{QStringLiteral("wlan9")});
  QVERIFY(!unknownDevice.allowed);
  QCOMPARE(unknownDevice.reasonCode, QStringLiteral("unknown-device"));

  const IntentVerdict idleDevice = validateDisconnect(
      validSnapshot(), DisconnectIntent{QStringLiteral("wlan0")});
  QVERIFY(!idleDevice.allowed);
  QCOMPARE(idleDevice.reasonCode, QStringLiteral("device-not-connected"));
}

void NetworkIntentPolicyTests::validatesSetRadioIntents() {
  const Snapshot snapshot = validSnapshot();
  QVERIFY(
      validateSetRadio(snapshot, SetRadioIntent{RadioKind::Wifi, false}).allowed);

  const IntentVerdict redundant =
      validateSetRadio(snapshot, SetRadioIntent{RadioKind::Wifi, true});
  QVERIFY(!redundant.allowed);
  QCOMPARE(redundant.reasonCode, QStringLiteral("radio-already-in-state"));

  Snapshot hardwareOff = validSnapshot();
  hardwareOff.radios = {Radio{RadioKind::Wifi, true, false, true}};
  const IntentVerdict blocked = validateSetRadio(
      hardwareOff, SetRadioIntent{RadioKind::Wifi, true});
  QVERIFY(!blocked.allowed);
  QCOMPARE(blocked.reasonCode, QStringLiteral("radio-hardware-disabled"));

  const IntentVerdict absent = validateSetRadio(
      snapshot, SetRadioIntent{RadioKind::Wwan, true});
  QVERIFY(!absent.allowed);
  QCOMPARE(absent.reasonCode, QStringLiteral("radio-absent"));
}

QTEST_MAIN(NetworkIntentPolicyTests)
#include "tst_network_intent_policy.moc"
