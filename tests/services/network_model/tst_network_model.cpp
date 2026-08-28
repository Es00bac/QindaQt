// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_protocol_test_data.h"

#include <qindaqt/services/network_model/network_model.h>

#include <QtTest>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Model;
using namespace QindaQt::Network::TestData;

class NetworkModelTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void acceptsFirstValidSnapshot();
  void rejectsInvalidCandidateAtomically();
  void rejectsStaleAndRolledBackSnapshotsAtomically();
  void ownerChangeWithNewEpochResetsState();
  void rejectsAbaReplayAtomically();
  void clearReportsUnavailableTruth();
  void publishesScanLeaseTruth();
  void marksBusyFromOperationInFlight();
  void redactsDiagnosticsOnProjection();
  void hidesHiddenSsidInDisplayName();
  void modelRejectsLateLowerRevisionAfterOwnerCycle();

private:
  qint64 m_now = 10'000;
  NetworkModel makeModel() {
    ++m_now;
    return NetworkModel([this] { return m_now; });
  }
};

void NetworkModelTests::acceptsFirstValidSnapshot() {
  NetworkModel model = makeModel();
  const NetworkModel::ApplyResult result = model.applySnapshot(validSnapshot());
  QVERIFY2(result.accepted, qPrintable(result.reasonCode));
  QVERIFY(model.snapshot().has_value());
  QCOMPARE(model.lineage()->owner, QStringLiteral(":1.23"));

  const ModelState projection = model.projection();
  QVERIFY(projection.hasSnapshot);
  QCOMPARE(projection.availability, Availability::Ready);
  QCOMPARE(projection.connectivity, ConnectivityKind::Full);
  QVERIFY(projection.scanCapable);
  QVERIFY(projection.radioControlCapable);
  QVERIFY(!projection.scanBusy);
}

void NetworkModelTests::rejectsInvalidCandidateAtomically() {
  NetworkModel model = makeModel();
  QVERIFY(model.applySnapshot(validSnapshot()).accepted);
  const ModelState before = model.projection();
  const auto lineageBefore = model.lineage();

  Snapshot invalid = validSnapshot();
  invalid.revision = 8;
  invalid.epoch = 0;
  const NetworkModel::ApplyResult result = model.applySnapshot(invalid);
  QVERIFY(!result.accepted);
  QVERIFY(!result.reasonCode.isEmpty());

  QCOMPARE(model.projection(), before);
  QCOMPARE(model.lineage(), lineageBefore);
}

void NetworkModelTests::rejectsStaleAndRolledBackSnapshotsAtomically() {
  NetworkModel model = makeModel();
  Snapshot accepted = validSnapshot();
  accepted.revision = 10;
  QVERIFY(model.applySnapshot(accepted).accepted);

  Snapshot stale = accepted;
  QVERIFY(!model.applySnapshot(stale).accepted);

  Snapshot rollback = accepted;
  rollback.revision = 9;
  QVERIFY(!model.applySnapshot(rollback).accepted);

  QCOMPARE(model.lineage()->revision, quint64(10));
}

void NetworkModelTests::ownerChangeWithNewEpochResetsState() {
  NetworkModel model = makeModel();
  Snapshot ownerA = validSnapshot();
  ownerA.scanPhase = ScanPhase::Leased;
  ownerA.scanLease = ScanLease{QStringLiteral("lease-a"), ownerA.epoch,
                                ownerA.revision, 1'000'000};
  QVERIFY(model.applySnapshot(ownerA).accepted);
  QVERIFY(model.scanLease().lease().has_value());

  Snapshot ownerB = validSnapshot();
  ownerB.owner = QStringLiteral(":1.42");
  ownerB.epoch = 42;
  ownerB.revision = 1;
  QVERIFY2(model.applySnapshot(ownerB).accepted, "new epoch resets lineage");
  QCOMPARE(model.lineage()->owner, QStringLiteral(":1.42"));
  QCOMPARE(model.lineage()->revision, quint64(1));
  QVERIFY(!model.scanLease().lease().has_value());
}

void NetworkModelTests::rejectsAbaReplayAtomically() {
  NetworkModel model = makeModel();
  Snapshot ownerA = validSnapshot();
  QVERIFY(model.applySnapshot(ownerA).accepted);

  Snapshot ownerB = validSnapshot();
  ownerB.owner = QStringLiteral(":1.42");
  ownerB.epoch = 42;
  QVERIFY(model.applySnapshot(ownerB).accepted);
  const ModelState before = model.projection();

  Snapshot replayedA = validSnapshot();
  replayedA.revision = 500;
  QVERIFY(!model.applySnapshot(replayedA).accepted);
  QCOMPARE(model.projection(), before);
  QCOMPARE(model.lineage()->owner, QStringLiteral(":1.42"));
}

void NetworkModelTests::clearReportsUnavailableTruth() {
  NetworkModel model = makeModel();
  QVERIFY(model.applySnapshot(validSnapshot()).accepted);
  model.clear();
  QVERIFY(!model.snapshot().has_value());
  QVERIFY(!model.lineage().has_value());

  const ModelState projection = model.projection();
  QVERIFY(!projection.hasSnapshot);
  QCOMPARE(projection.availability, Availability::Unavailable);
  QCOMPARE(projection.reasonCode, QStringLiteral("no-snapshot"));

  model.clear();
  QVERIFY(!model.snapshot().has_value());
}

void NetworkModelTests::publishesScanLeaseTruth() {
  NetworkModel model = makeModel();
  Snapshot leased = leasedSnapshot(QStringLiteral("lease-1"));
  // Deadline 120'000 against a fake clock at m_now; make it finite relative.
  leased.scanLease.deadlineEpochMs = m_now + 30'000;
  QVERIFY2(model.applySnapshot(leased).accepted, "lease adopted");
  QVERIFY(model.scanLease().lease().has_value());

  ModelState projection = model.projection();
  QCOMPARE(projection.scanPhase, ScanPhase::Leased);
  QCOMPARE(projection.scanLeaseRemainingMs, 30'000);
  QVERIFY(!projection.scanLeaseExpired);
  QVERIFY(!projection.scanBusy);

  m_now += 31'000;
  projection = model.projection();
  QVERIFY(projection.scanLeaseExpired);
  QCOMPARE(projection.scanLeaseRemainingMs, qint64(0));

  Snapshot scanning = validSnapshot();
  // The gate demands a strictly newer revision within the same lineage.
  scanning.revision = leased.revision + 1;
  scanning.scanPhase = ScanPhase::Scanning;
  scanning.scanLease = leased.scanLease;
  QVERIFY(model.applySnapshot(scanning).accepted);
  QVERIFY(model.projection().scanBusy);
}

void NetworkModelTests::marksBusyFromOperationInFlight() {
  NetworkModel model = makeModel();
  QVERIFY(model.applySnapshot(validSnapshot()).accepted);
  QVERIFY(!model.projection().scanBusy);
  QVERIFY(model.projection(true).scanBusy);
}

void NetworkModelTests::redactsDiagnosticsOnProjection() {
  NetworkModel model = makeModel();
  Snapshot degraded = validSnapshot();
  degraded.availability = Availability::Degraded;
  degraded.reasonCode = QStringLiteral("partial-inventory");
  degraded.diagnostic =
      QStringLiteral("device fetch failed: password=hunter2 ssid=Home");
  QVERIFY(model.applySnapshot(degraded).accepted);

  const ModelState projection = model.projection();
  QCOMPARE(projection.reasonCode, QStringLiteral("partial-inventory"));
  QVERIFY(projection.diagnostic.contains(QStringLiteral("password=<redacted>")));
  QVERIFY(projection.diagnostic.contains(QStringLiteral("ssid=Home")));
  QVERIFY(!projection.diagnostic.contains(QStringLiteral("hunter2")));
}

void NetworkModelTests::hidesHiddenSsidInDisplayName() {
  AccessPoint hidden;
  hidden.deviceInterface = QStringLiteral("wlan0");
  hidden.hidden = true;
  hidden.bssid = QStringLiteral("aa:2b:3c:4d:5e:6f");
  QCOMPARE(accessPointDisplayName(hidden), QStringLiteral("Hidden network"));

  AccessPoint visible;
  visible.deviceInterface = QStringLiteral("wlan0");
  visible.ssid = QStringLiteral("Cafe");
  visible.bssid = QStringLiteral("aa:2b:3c:4d:5e:6f");
  QCOMPARE(accessPointDisplayName(visible), QStringLiteral("Cafe"));
}

void NetworkModelTests::modelRejectsLateLowerRevisionAfterOwnerCycle() {
  NetworkModel model = makeModel();
  Snapshot a1 = validSnapshot();
  a1.revision = 5;
  QVERIFY(model.applySnapshot(a1).accepted);

  Snapshot b1 = validSnapshot();
  b1.owner = QStringLiteral(":1.42");
  b1.epoch = 42;
  b1.revision = 1;
  QVERIFY(model.applySnapshot(b1).accepted);

  // Late in-flight reply from B's earlier revision is stale within B itself.
  Snapshot b0 = b1;
  QVERIFY(!model.applySnapshot(b0).accepted);
  QCOMPARE(model.lineage()->revision, quint64(1));
}

QTEST_MAIN(NetworkModelTests)
#include "tst_network_model.moc"
