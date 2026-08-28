// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_protocol_test_data.h"

#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_protocol/network_validation.h>

#include <QtTest>

using namespace QindaQt::Network;
using namespace QindaQt::Network::TestData;

class NetworkValidationTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void acceptsCanonicalSnapshot();
  void rejectsLineageViolations();
  void rejectsUnknownEnumsAndCapabilities();
  void requiresReasonForUnavailableAndDegraded();
  void rejectsDuplicateAndCappedCollections();
  void rejectsBadAccessPointIdentity();
  void rejectsBadKnownNetworkIdentity();
  void rejectsActiveConnectionReferencingMissingTruth();
  void rejectsScanLeaseInconsistencies();
  void validatesOperationResultTruth();
};

void NetworkValidationTests::acceptsCanonicalSnapshot() {
  const ValidationResult result = validateSnapshot(validSnapshot());
  QVERIFY2(result.accepted, qPrintable(result.reasonCode));

  Snapshot leased = leasedSnapshot();
  QVERIFY2(validateSnapshot(leased).accepted,
           qPrintable(validateSnapshot(leased).reasonCode));
}

void NetworkValidationTests::rejectsLineageViolations() {
  Snapshot zeroEpoch = validSnapshot();
  zeroEpoch.epoch = 0;
  QVERIFY(!validateSnapshot(zeroEpoch).accepted);

  Snapshot zeroRevision = validSnapshot();
  zeroRevision.revision = 0;
  QVERIFY(!validateSnapshot(zeroRevision).accepted);

  Snapshot badVersion = validSnapshot();
  badVersion.protocolVersion = kProtocolVersion + 1;
  QVERIFY(!validateSnapshot(badVersion).accepted);

  Snapshot emptyOwnerWhileReady = validSnapshot();
  emptyOwnerWhileReady.owner = QString();
  QVERIFY(!validateSnapshot(emptyOwnerWhileReady).accepted);

  Snapshot oversizedOwner = validSnapshot();
  oversizedOwner.owner = QString(kMaxOwnerUtf8Bytes + 1, u':');
  QVERIFY(!validateSnapshot(oversizedOwner).accepted);
}

void NetworkValidationTests::rejectsUnknownEnumsAndCapabilities() {
  Snapshot unknownAvailability = validSnapshot();
  unknownAvailability.availability = static_cast<Availability>(9);
  QVERIFY(!validateSnapshot(unknownAvailability).accepted);

  Snapshot unknownConnectivity = validSnapshot();
  unknownConnectivity.connectivity = static_cast<ConnectivityKind>(8);
  QVERIFY(!validateSnapshot(unknownConnectivity).accepted);

  Snapshot unknownCapability;
  Snapshot unknownCapabilitySnapshot = validSnapshot();
  unknownCapabilitySnapshot.capabilities =
      Capabilities(static_cast<Capability>(1U << 20U));
  QVERIFY(!validateSnapshot(unknownCapabilitySnapshot).accepted);
  Q_UNUSED(unknownCapability);
}

void NetworkValidationTests::requiresReasonForUnavailableAndDegraded() {
  Snapshot unavailable = validSnapshot();
  unavailable.availability = Availability::Unavailable;
  QVERIFY(!validateSnapshot(unavailable).accepted);
  unavailable.reasonCode = QStringLiteral("upstream-absent");
  QVERIFY2(validateSnapshot(unavailable).accepted,
           qPrintable(validateSnapshot(unavailable).reasonCode));

  Snapshot degraded = validSnapshot();
  degraded.availability = Availability::Degraded;
  QVERIFY(!validateSnapshot(degraded).accepted);
  degraded.reasonCode = QStringLiteral("partial-inventory");
  QVERIFY(validateSnapshot(degraded).accepted);

  Snapshot oversizedDiagnostic = validSnapshot();
  oversizedDiagnostic.diagnostic = QString(kMaxDiagnosticUtf8Bytes + 8, u'd');
  QVERIFY(!validateSnapshot(oversizedDiagnostic).accepted);
}

void NetworkValidationTests::rejectsDuplicateAndCappedCollections() {
  Snapshot duplicateRadio = validSnapshot();
  duplicateRadio.radios.append(Radio{RadioKind::Wifi, false, true, true});
  QVERIFY(!validateSnapshot(duplicateRadio).accepted);

  Snapshot duplicateInterface = validSnapshot();
  duplicateInterface.devices.append(wifiDevice());
  QVERIFY(!validateSnapshot(duplicateInterface).accepted);

  Snapshot cappedRadios = validSnapshot();
  for (int index = 0; index <= kMaxRadios; ++index) {
    cappedRadios.radios.append(Radio{static_cast<RadioKind>(index % 2), true,
                                     true, true});
  }
  QVERIFY(!validateSnapshot(cappedRadios).accepted);

  Snapshot cappedNetworks = validSnapshot();
  for (int index = 0; index <= kMaxKnownNetworks; ++index) {
    const QByteArray ssid = "net" + QByteArray::number(index);
    cappedNetworks.knownNetworks.append(
        KnownNetwork{knownNetworkId(ssid, SecuritySuite::Open),
                     QStringLiteral("net") + QString::number(index), false,
                     SecuritySuite::Open, true});
  }
  QVERIFY(!validateSnapshot(cappedNetworks).accepted);
}

void NetworkValidationTests::rejectsBadAccessPointIdentity() {
  Snapshot missingDevice = validSnapshot();
  missingDevice.accessPoints = {AccessPoint{
      QStringLiteral("wlan9"), QStringLiteral("Cafe"), false,
      QStringLiteral("aa:2b:3c:4d:5e:6f"), SecuritySuite::Wpa2Personal, 2'412,
      60}};
  QVERIFY(!validateSnapshot(missingDevice).accepted);

  Snapshot wrongDeviceKind = validSnapshot();
  wrongDeviceKind.accessPoints = {AccessPoint{
      QStringLiteral("enp3s0"), QStringLiteral("Cafe"), false,
      QStringLiteral("aa:2b:3c:4d:5e:6f"), SecuritySuite::Wpa2Personal, 2'412,
      60}};
  QVERIFY(!validateSnapshot(wrongDeviceKind).accepted);

  Snapshot badBssid = validSnapshot();
  badBssid.accessPoints = {AccessPoint{
      QStringLiteral("wlan0"), QStringLiteral("Cafe"), false,
      QStringLiteral("AA:2B:3C:4D:5E:6F"), SecuritySuite::Wpa2Personal, 2'412,
      60}};
  QVERIFY(!validateSnapshot(badBssid).accepted);

  Snapshot hiddenWithText = validSnapshot();
  hiddenWithText.accessPoints = {AccessPoint{
      QStringLiteral("wlan0"), QStringLiteral("Cafe"), true,
      QStringLiteral("aa:2b:3c:4d:5e:6f"), SecuritySuite::Wpa2Personal, 2'412,
      60}};
  QVERIFY(!validateSnapshot(hiddenWithText).accepted);

  Snapshot badSignal = validSnapshot();
  badSignal.accessPoints = {AccessPoint{
      QStringLiteral("wlan0"), QStringLiteral("Cafe"), false,
      QStringLiteral("aa:2b:3c:4d:5e:6f"), SecuritySuite::Wpa2Personal, 2'412,
      kMaximumSignalStrength + 1}};
  QVERIFY(!validateSnapshot(badSignal).accepted);

  Snapshot badFrequency = validSnapshot();
  badFrequency.accessPoints = {AccessPoint{
      QStringLiteral("wlan0"), QStringLiteral("Cafe"), false,
      QStringLiteral("aa:2b:3c:4d:5e:6f"), SecuritySuite::Wpa2Personal, 9'999,
      60}};
  QVERIFY(!validateSnapshot(badFrequency).accepted);
}

void NetworkValidationTests::rejectsBadKnownNetworkIdentity() {
  Snapshot attackerId = validSnapshot();
  attackerId.knownNetworks = {KnownNetwork{
      QStringLiteral("not-a-derived-id"), QStringLiteral("Cafe"), false,
      SecuritySuite::Wpa2Personal, true}};
  QVERIFY(!validateSnapshot(attackerId).accepted);

  Snapshot hiddenWithText = validSnapshot();
  hiddenWithText.knownNetworks = {KnownNetwork{
      knownNetworkId(QByteArray("x"), SecuritySuite::Open),
      QStringLiteral("x"), true, SecuritySuite::Open, true}};
  QVERIFY(!validateSnapshot(hiddenWithText).accepted);

  Snapshot controlCharSsid = validSnapshot();
  controlCharSsid.knownNetworks = {KnownNetwork{
      knownNetworkId(QByteArray("x"), SecuritySuite::Open),
      QStringLiteral("x\01"), false, SecuritySuite::Open, true}};
  QVERIFY(!validateSnapshot(controlCharSsid).accepted);
}

void NetworkValidationTests::rejectsActiveConnectionReferencingMissingTruth() {
  Snapshot missingDevice = validSnapshot();
  missingDevice.activeConnections = {
      ActiveConnection{QStringLiteral("wlan9"), cafeNetwork().id}};
  QVERIFY(!validateSnapshot(missingDevice).accepted);

  Snapshot missingNetwork = validSnapshot();
  missingNetwork.activeConnections = {
      ActiveConnection{QStringLiteral("wlan0"), QStringLiteral(
          "0000000000000000000000000000000000000000000000000000000000000000")}};
  QVERIFY(!validateSnapshot(missingNetwork).accepted);

  Snapshot duplicateDevice = validSnapshot();
  duplicateDevice.activeConnections = {
      ActiveConnection{QStringLiteral("wlan0"), cafeNetwork().id},
      ActiveConnection{QStringLiteral("wlan0"), cafeNetwork().id}};
  QVERIFY(!validateSnapshot(duplicateDevice).accepted);
}

void NetworkValidationTests::rejectsScanLeaseInconsistencies() {
  Snapshot idleWithLease = validSnapshot();
  idleWithLease.scanPhase = ScanPhase::Idle;
  idleWithLease.scanLease = ScanLease{QStringLiteral("lease-1"), 41, 7, 1'000};
  QVERIFY(!validateSnapshot(idleWithLease).accepted);

  Snapshot leaseWithZeroDeadline = leasedSnapshot();
  leaseWithZeroDeadline.scanLease.deadlineEpochMs = 0;
  QVERIFY(!validateSnapshot(leaseWithZeroDeadline).accepted);

  Snapshot leaseFromOtherEpoch = leasedSnapshot();
  leaseFromOtherEpoch.scanLease.grantedEpoch = 40;
  QVERIFY(!validateSnapshot(leaseFromOtherEpoch).accepted);

  Snapshot leaseFromFutureRevision = leasedSnapshot();
  leaseFromFutureRevision.scanLease.grantedRevision = 8;
  QVERIFY(!validateSnapshot(leaseFromFutureRevision).accepted);

  Snapshot leaseWithEmptyId = leasedSnapshot();
  leaseWithEmptyId.scanLease.leaseId = QString();
  QVERIFY(!validateSnapshot(leaseWithEmptyId).accepted);
}

void NetworkValidationTests::validatesOperationResultTruth() {
  QVERIFY2(validateOperationResult(validOperationResult()).accepted, "valid");

  OperationResult zeroLineage = validOperationResult();
  zeroLineage.initiatingEpoch = 0;
  QVERIFY(!validateOperationResult(zeroLineage).accepted);

  OperationResult unknownKind = validOperationResult();
  unknownKind.kind = static_cast<OperationKind>(12);
  QVERIFY(!validateOperationResult(unknownKind).accepted);

  OperationResult rejectionWithoutReason = validOperationResult(
      OperationKind::ConnectKnownNetwork, OperationStatus::Rejected);
  rejectionWithoutReason.reasonCode = QString();
  QVERIFY(!validateOperationResult(rejectionWithoutReason).accepted);

  OperationResult succeededWithReason = validOperationResult();
  succeededWithReason.reasonCode = QStringLiteral("ok-reason");
  QVERIFY(validateOperationResult(succeededWithReason).accepted);
}

QTEST_MAIN(NetworkValidationTests)
#include "tst_network_validation.moc"
