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
  void validatesVisibleConnectIntents();
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

  Snapshot withoutVisible = validSnapshot();
  withoutVisible.capabilities &=
      ~Capabilities(Capability::VisibleNetworkControl);
  const IntentVerdict visible = validateConnectVisible(
      withoutVisible, ConnectVisibleIntent{QString(64, u'a')});
  QCOMPARE(visible.reasonCode,
           QStringLiteral("visible-network-control-unsupported"));

  Snapshot withoutRadio = validSnapshot();
  withoutRadio.capabilities &= ~Capabilities(Capability::RadioControl);
  QVERIFY(!validateSetRadio(withoutRadio,
                            SetRadioIntent{RadioKind::Wifi, false})
               .allowed);
}

void NetworkIntentPolicyTests::validatesVisibleConnectIntents() {
  const auto point = [](const SecuritySuite security, const bool hidden = false) {
    return AccessPoint{QStringLiteral("wlan0"),
                       hidden ? QString{} : QStringLiteral("New network"),
                       hidden, QStringLiteral("02:11:22:33:44:55"), security,
                       5'180, 72};
  };
  for (const SecuritySuite security : {SecuritySuite::Open,
                                       SecuritySuite::Wpa2Personal,
                                       SecuritySuite::Wpa3Personal}) {
    Snapshot snapshot = validSnapshot();
    snapshot.accessPoints = {point(security)};
    const QString id = visibleAccessPointId(
        snapshot.accessPoints.first().deviceInterface,
        snapshot.accessPoints.first().bssid);
    QVERIFY2(validateConnectVisible(snapshot, ConnectVisibleIntent{id}).allowed,
             qPrintable(QString::number(static_cast<quint32>(security))));
  }

  for (const SecuritySuite security : {SecuritySuite::Wep,
                                       SecuritySuite::Wpa2Enterprise,
                                       SecuritySuite::Wpa3Enterprise}) {
    Snapshot snapshot = validSnapshot();
    snapshot.accessPoints = {point(security)};
    const QString id = visibleAccessPointId(
        snapshot.accessPoints.first().deviceInterface,
        snapshot.accessPoints.first().bssid);
    const IntentVerdict verdict =
        validateConnectVisible(snapshot, ConnectVisibleIntent{id});
    QVERIFY(!verdict.allowed);
    QVERIFY(verdict.reasonCode.endsWith(QStringLiteral("unsupported")));
  }

  Snapshot hidden = validSnapshot();
  hidden.accessPoints = {point(SecuritySuite::Open, true)};
  const QString hiddenId = visibleAccessPointId(
      hidden.accessPoints.first().deviceInterface,
      hidden.accessPoints.first().bssid);
  QCOMPARE(validateConnectVisible(hidden, ConnectVisibleIntent{hiddenId})
               .reasonCode,
           QStringLiteral("hidden-network-unsupported"));

  Snapshot known = validSnapshot();
  known.accessPoints = {{QStringLiteral("wlan0"), cafeNetwork().ssid, false,
                         QStringLiteral("02:11:22:33:44:55"),
                         cafeNetwork().security, 5'180, 72}};
  const QString knownId = visibleAccessPointId(
      known.accessPoints.first().deviceInterface,
      known.accessPoints.first().bssid);
  QCOMPARE(validateConnectVisible(known, ConnectVisibleIntent{knownId})
               .reasonCode,
           QStringLiteral("network-already-known"));
  QCOMPARE(validateConnectVisible(validSnapshot(),
                                  ConnectVisibleIntent{QString(64, u'a')})
               .reasonCode,
           QStringLiteral("unknown-access-point"));
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
  QVERIFY(held.adopt(ScanLease{QStringLiteral("lease-1"), 41, 7, 120'000},
                     41, at(0)));
  Snapshot leased = leasedSnapshot();
  const IntentVerdict heldVerdict =
      validateRequestScan(leased, held, at(1'000), RequestScanIntent{30'000});
  QVERIFY(!heldVerdict.allowed);
  QCOMPARE(heldVerdict.reasonCode, QStringLiteral("scan-lease-held"));

  // An expired lease no longer pins the scan result set.
  const IntentVerdict afterExpiry =
      validateRequestScan(leased, held, at(120'000), RequestScanIntent{30'000});
  QVERIFY2(afterExpiry.allowed, qPrintable(afterExpiry.reasonCode));
}

void NetworkIntentPolicyTests::validatesConnectIntents() {
  const Snapshot snapshot = validSnapshot();
  QVERIFY(validateConnect(snapshot, ConnectIntent{cafeNetwork().id}).allowed);

  const IntentVerdict unknown =
      validateConnect(snapshot, ConnectIntent{QStringLiteral("deadbeef")});
  QVERIFY(!unknown.allowed);
  QCOMPARE(unknown.reasonCode, QStringLiteral("known-network-id-invalid"));

  const IntentVerdict oversized = validateConnect(
      snapshot, ConnectIntent{QString(kMaxNetworkIdUtf8Bytes + 1, u'a')});
  QVERIFY(!oversized.allowed);
  QCOMPARE(oversized.reasonCode, QStringLiteral("known-network-id-invalid"));

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

  const IntentVerdict oversizedDevice = validateDisconnect(
      snapshot,
      DisconnectIntent{QString(kMaxInterfaceUtf8Bytes + 1, u'w')});
  QVERIFY(!oversizedDevice.allowed);
  QCOMPARE(oversizedDevice.reasonCode,
           QStringLiteral("device-interface-invalid"));

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

  const IntentVerdict invalidKind = validateSetRadio(
      snapshot, SetRadioIntent{static_cast<RadioKind>(99), true});
  QVERIFY(!invalidKind.allowed);
  QCOMPARE(invalidKind.reasonCode, QStringLiteral("radio-kind-invalid"));
}

QTEST_MAIN(NetworkIntentPolicyTests)
#include "tst_network_intent_policy.moc"
