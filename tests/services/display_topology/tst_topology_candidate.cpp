// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_topology/topology.h>

#include "support/topology_test_data.h"

#include <QtTest>

using namespace QindaQt::DisplayTopology;
namespace Display = QindaQt::Display;

class TopologyCandidateTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void enabledPrimaryModeAndPriorityInvariants();
    void mirrorRelationshipsAreClosedAndCanonical();
    void diffNoOpAndFingerprintAreStable();
    void fractionalWarningNamesTheOutput();
};

void TopologyCandidateTests::enabledPrimaryModeAndPriorityInvariants()
{
    const Display::Snapshot snapshot = Test::dualSnapshot();
    Display::Candidate candidate = Test::candidate(snapshot);
    candidate.outputs[0].enabled = false;
    candidate.outputs[0].primary = false;
    candidate.outputs[0].priority = 0;
    candidate.outputs[1].enabled = false;
    candidate.outputs[1].priority = 0;
    QCOMPARE(validateAndNormalize(snapshot, candidate).error,
             TopologyError::AllOutputsDisabled);

    candidate = Test::candidate(snapshot);
    candidate.outputs[1].primary = true;
    QCOMPARE(validateAndNormalize(snapshot, candidate).error, TopologyError::InvalidPrimary);
    candidate = Test::candidate(snapshot);
    candidate.outputs[1].priority = 1;
    QCOMPARE(validateAndNormalize(snapshot, candidate).error, TopologyError::InvalidPriority);
    candidate = Test::candidate(snapshot);
    candidate.outputs[1].modeId = QStringLiteral("unknown");
    QCOMPARE(validateAndNormalize(snapshot, candidate).error, TopologyError::UnknownMode);
    candidate = Test::candidate(snapshot);
    candidate.outputs.removeLast();
    QCOMPARE(validateAndNormalize(snapshot, candidate).error,
             TopologyError::OutputSetMismatch);
}

void TopologyCandidateTests::mirrorRelationshipsAreClosedAndCanonical()
{
    const Display::Snapshot snapshot = Test::dualSnapshot();
    Display::Candidate candidate = Test::candidate(snapshot);
    candidate.outputs[1].replicationSourceStableId = candidate.outputs[1].stableId;
    QCOMPARE(validateAndNormalize(snapshot, candidate).error,
             TopologyError::MirrorSelfReference);
    candidate = Test::candidate(snapshot);
    candidate.outputs[1].replicationSourceStableId = QStringLiteral("missing");
    QCOMPARE(validateAndNormalize(snapshot, candidate).error,
             TopologyError::UnknownMirrorSource);
    candidate = Test::candidate(snapshot);
    candidate.outputs[0].replicationSourceStableId = candidate.outputs[1].stableId;
    candidate.outputs[1].replicationSourceStableId = candidate.outputs[0].stableId;
    QCOMPARE(validateAndNormalize(snapshot, candidate).error, TopologyError::MirrorCycle);

    candidate = Test::candidate(snapshot);
    candidate.outputs[1].replicationSourceStableId = candidate.outputs[0].stableId;
    candidate.outputs[1].position = QPoint(999, 700);
    candidate.outputs[1].scale = 2.0;
    candidate.outputs[1].transform = Display::Transform::Rotate90;
    const ValidationResult mirrored = validateAndNormalize(snapshot, candidate);
    QVERIFY2(mirrored.accepted(), qPrintable(mirrored.reasonCode));
    QCOMPARE(mirrored.geometries[0].logicalRect, mirrored.geometries[1].logicalRect);

    Display::Candidate otherDerivedValues = candidate;
    otherDerivedValues.outputs[1].position = QPoint(100, 50);
    otherDerivedValues.outputs[1].scale = 1.25;
    otherDerivedValues.outputs[1].transform = Display::Transform::Rotate90;
    const ValidationResult second = validateAndNormalize(snapshot, otherDerivedValues);
    QVERIFY(second.accepted());
    QCOMPARE(second.fingerprint, mirrored.fingerprint);

    Display::Candidate perOutputChange = otherDerivedValues;
    perOutputChange.outputs[1].transform = Display::Transform::Normal;
    const ValidationResult changed = validateAndNormalize(snapshot, perOutputChange);
    QVERIFY(changed.accepted());
    QVERIFY(changed.fingerprint != mirrored.fingerprint);

    Display::Snapshot liveMirror = snapshot;
    liveMirror.outputs[1].replicationSourceStableId = liveMirror.outputs[0].stableId;
    liveMirror.outputs[1].position = QPoint(800, 500);
    liveMirror.outputs[1].scale = 2.0;
    liveMirror.liveFingerprint = canonicalFingerprint(candidateFromSnapshot(liveMirror));
    const ValidationResult baseline = validateAndNormalize(
        liveMirror, candidateFromSnapshot(liveMirror));
    QVERIFY(baseline.accepted());
    QVERIFY(baseline.noOp);
    QCOMPARE(baseline.fingerprint, liveMirror.liveFingerprint);
}

void TopologyCandidateTests::diffNoOpAndFingerprintAreStable()
{
    const Display::Snapshot snapshot = Test::dualSnapshot();
    Display::Candidate candidate = Test::candidate(snapshot);
    ValidationResult result = validateAndNormalize(snapshot, candidate);
    QVERIFY(result.accepted());
    QVERIFY(result.noOp);
    QVERIFY(result.differences.isEmpty());
    QCOMPARE(result.fingerprint, snapshot.liveFingerprint);

    std::reverse(candidate.outputs.begin(), candidate.outputs.end());
    const ValidationResult reordered = validateAndNormalize(snapshot, candidate);
    QVERIFY(reordered.accepted());
    QCOMPARE(reordered.fingerprint, result.fingerprint);

    candidate = Test::candidate(snapshot);
    candidate.outputs[1].scale = 1.25;
    result = validateAndNormalize(snapshot, candidate);
    QVERIFY(result.accepted());
    QVERIFY(!result.noOp);
    QCOMPARE(result.differences.size(), 1);
    QCOMPARE(result.differences.first().fields, QList{DiffField::Scale});
    QVERIFY(result.fingerprint != snapshot.liveFingerprint);
}

void TopologyCandidateTests::fractionalWarningNamesTheOutput()
{
    Display::Snapshot snapshot = Test::singleSnapshot();
    Display::Candidate candidate = Test::candidate(snapshot);
    candidate.outputs[0].scale = 1.5;
    const ValidationResult result = validateAndNormalize(snapshot, candidate);
    QVERIFY(result.accepted());
    QVERIFY(std::any_of(result.warnings.cbegin(), result.warnings.cend(),
                        [&](const TopologyWarning &warning) {
                            return warning.kind
                                    == TopologyWarningKind::NonIntegralLogicalExtent
                                && warning.stableId == candidate.outputs[0].stableId;
                        }));
}

QTEST_GUILESS_MAIN(TopologyCandidateTests)
#include "tst_topology_candidate.moc"
