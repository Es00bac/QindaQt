// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_protocol_test_data.h"

#include <qindaqt/services/network_model/network_snapshot_gate.h>

#include <QtTest>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Model;
using namespace QindaQt::Network::TestData;

class NetworkSnapshotGateTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void acceptsFirstValidLineage();
  void requiresNonzeroLineage();
  void acceptsStrictlyIncreasingRevisions();
  void rejectsStaleRevision();
  void rejectsRevisionRollback();
  void acceptsOwnerChangeWithNewerEpoch();
  void rejectsOwnerChangeWithoutNewerEpoch();
  void rejectsAbaOwnerReplay();
  void rejectsSameOwnerEpochReset();
};

void NetworkSnapshotGateTests::acceptsFirstValidLineage() {
  const GateDecision decision = gateSnapshot(std::nullopt, validSnapshot());
  QVERIFY2(decision.accepted(), qPrintable(decision.reasonCode));
}

void NetworkSnapshotGateTests::requiresNonzeroLineage() {
  Snapshot zeroEpoch = validSnapshot();
  zeroEpoch.epoch = 0;
  QCOMPARE(gateSnapshot(std::nullopt, zeroEpoch).kind,
           GateDecisionKind::RejectInvalidLineage);

  Snapshot zeroRevision = validSnapshot();
  zeroRevision.revision = 0;
  QCOMPARE(gateSnapshot(std::nullopt, zeroRevision).kind,
           GateDecisionKind::RejectInvalidLineage);
}

void NetworkSnapshotGateTests::acceptsStrictlyIncreasingRevisions() {
  const Lineage current{QStringLiteral(":1.23"), 41, 7};
  Snapshot next = validSnapshot();
  next.revision = 8;
  QVERIFY(gateSnapshot(current, next).accepted());

  next.revision = 1'000;
  QVERIFY(gateSnapshot(current, next).accepted());
}

void NetworkSnapshotGateTests::rejectsStaleRevision() {
  const Lineage current{QStringLiteral(":1.23"), 41, 7};
  Snapshot sameRevision = validSnapshot();
  sameRevision.revision = 7;
  const GateDecision decision = gateSnapshot(current, sameRevision);
  QCOMPARE(decision.kind, GateDecisionKind::RejectStaleRevision);
  QVERIFY(!decision.reasonCode.isEmpty());
}

void NetworkSnapshotGateTests::rejectsRevisionRollback() {
  const Lineage current{QStringLiteral(":1.23"), 41, 100};
  Snapshot rollback = validSnapshot();
  rollback.revision = 99;
  QCOMPARE(gateSnapshot(current, rollback).kind,
           GateDecisionKind::RejectStaleRevision);
}

void NetworkSnapshotGateTests::acceptsOwnerChangeWithNewerEpoch() {
  const Lineage ownerA{QStringLiteral(":1.23"), 41, 100};
  Snapshot ownerB = validSnapshot();
  ownerB.owner = QStringLiteral(":1.42");
  ownerB.epoch = 42;
  ownerB.revision = 1;
  QVERIFY2(gateSnapshot(ownerA, ownerB).accepted(), "new epoch may reset revision");
}

void NetworkSnapshotGateTests::rejectsOwnerChangeWithoutNewerEpoch() {
  const Lineage ownerA{QStringLiteral(":1.23"), 41, 100};
  Snapshot ownerB = validSnapshot();
  ownerB.owner = QStringLiteral(":1.42");
  ownerB.epoch = 41;
  QCOMPARE(gateSnapshot(ownerA, ownerB).kind, GateDecisionKind::RejectStaleEpoch);

  Snapshot ownerBOlder = validSnapshot();
  ownerBOlder.owner = QStringLiteral(":1.42");
  ownerBOlder.epoch = 40;
  QCOMPARE(gateSnapshot(ownerA, ownerBOlder).kind,
           GateDecisionKind::RejectStaleEpoch);
}

void NetworkSnapshotGateTests::rejectsAbaOwnerReplay() {
  // A owns epoch 41, B replaces it with epoch 42, then a delayed A snapshot
  // replays epoch 41. The gate must refuse the replay regardless of content.
  const Lineage ownerB{QStringLiteral(":1.42"), 42, 3};
  Snapshot replayedA = validSnapshot();
  replayedA.owner = QStringLiteral(":1.23");
  replayedA.epoch = 41;
  replayedA.revision = 500;
  QCOMPARE(gateSnapshot(ownerB, replayedA).kind, GateDecisionKind::RejectStaleEpoch);
}

void NetworkSnapshotGateTests::rejectsSameOwnerEpochReset() {
  const Lineage current{QStringLiteral(":1.23"), 41, 7};
  Snapshot reset = validSnapshot();
  reset.epoch = 42;
  QCOMPARE(gateSnapshot(current, reset).kind,
           GateDecisionKind::RejectSameOwnerEpochReset);
}

QTEST_MAIN(NetworkSnapshotGateTests)
#include "tst_network_snapshot_gate.moc"
