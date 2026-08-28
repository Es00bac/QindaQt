// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_service/display_service_model.h>

#include <qindaqt/services/display_protocol/display_validation.h>
#include <qindaqt/services/display_topology/topology.h>

#include "display_service_projection_p.h"
#include "support/display_service_test_support.h"

#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayService;
using namespace QindaQt::DisplayService::TestSupport;

class DisplayServiceModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fencesOwnerGenerationAndTransportLoss();
    void routesAddRemoveChangeAndFencesStaleCandidate();
    void ownsPreviewConfirmAndRevertTransitions();
    void fencesLateCompletionAcrossMachineReplacement();
    void projectsZeroOrOneValidatedTransactionSummary();
    void projectsRevertAndStuckSummaries();
    void projectionFailsClosedOnInvalidView();
};

void DisplayServiceModelTest::fencesOwnerGenerationAndTransportLoss()
{
    FakeClock clock;
    FakeTransactionPort port;
    QStringList epochs{QStringLiteral("epoch-a"), QStringLiteral("epoch-b"),
                       QStringLiteral("epoch-a")};
    DisplayServiceModel model(clock, port, [&epochs] { return epochs.takeFirst(); });

    const InventoryFrame initial = frame(1, {output()});
    QCOMPARE(model.observeInventory(initial).status,
             InventoryObservationStatus::AcceptedNewLineage);
    QVERIFY(model.available());
    const QString firstEpoch = model.snapshot()->serviceEpoch;
    QVERIFY(!firstEpoch.isEmpty());
    const Display::Candidate staleFirst =
        DisplayTopology::candidateFromSnapshot(*model.snapshot());
    QCOMPARE(model.snapshot()->revision, quint64(1));
    QCOMPARE(model.machineLineage(), quint64(1));
    QCOMPARE(model.observeInventory(initial).status,
             InventoryObservationStatus::AcceptedUnchanged);
    InventoryFrame unjustifiedGeneration = initial;
    unjustifiedGeneration.outputGeneration = 2;
    QCOMPARE(model.observeInventory(unjustifiedGeneration).reasonCode,
             QStringLiteral("unchanged-new-output-generation"));
    QCOMPARE(model.snapshot()->revision, quint64(1));

    InventoryFrame collision = initial;
    collision.outputs[0].model = QStringLiteral("Changed at same generation");
    const InventoryObservationResult rejected = model.observeInventory(collision);
    QCOMPARE(rejected.status, InventoryObservationStatus::Rejected);
    QCOMPARE(rejected.reasonCode, QStringLiteral("changed-equal-output-generation"));
    QCOMPARE(model.snapshot()->outputs.constFirst().model,
             QStringLiteral("Reference Display"));

    const InventoryFrame replacement = frame(1, {output()}, QStringLiteral(":1.77"));
    QCOMPARE(model.observeInventory(replacement).status,
             InventoryObservationStatus::AcceptedNewLineage);
    const QString secondEpoch = model.snapshot()->serviceEpoch;
    QVERIFY(secondEpoch != firstEpoch);
    QCOMPARE(model.snapshot()->revision, quint64(1));
    QCOMPARE(model.machineLineage(), quint64(2));
    QVERIFY(model.transportLost());
    QVERIFY(!model.available());
    QVERIFY(model.snapshot() == nullptr);
    const InventoryFrame recovered = frame(1, {output()}, QStringLiteral(":1.88"));
    QCOMPARE(model.observeInventory(recovered).status,
             InventoryObservationStatus::AcceptedNewLineage);
    const QString thirdEpoch = model.snapshot()->serviceEpoch;
    QVERIFY(thirdEpoch != firstEpoch);
    QVERIFY(thirdEpoch != secondEpoch);
    QCOMPARE(model.machineLineage(), quint64(3));

    QCOMPARE(model.stage(QStringLiteral("stale-first"), staleFirst).command.error,
             DisplayTransaction::CommandError::StaleRevision);
}

void DisplayServiceModelTest::routesAddRemoveChangeAndFencesStaleCandidate()
{
    FakeClock clock;
    FakeTransactionPort port;
    DisplayServiceModel model(clock, port, [] { return QStringLiteral("epoch-a"); });
    QVERIFY(model.observeInventory(frame(1, {output()})).accepted());
    const Display::Candidate stale =
        DisplayTopology::candidateFromSnapshot(*model.snapshot());

    InventoryOutput second = output(QStringLiteral("HDMI-A-1"),
                                    QRect(1920, 0, 1280, 720));
    second.runtimeCompositorUuid = QStringLiteral("runtime-2");
    QCOMPARE(model.observeInventory(frame(2, {output(), second})).status,
             InventoryObservationStatus::AcceptedChanged);
    QCOMPARE(model.snapshot()->outputs.size(), 2);
    QCOMPARE(model.stage(QStringLiteral("stale"), stale).command.error,
             DisplayTransaction::CommandError::StaleRevision);

    second.model = QStringLiteral("Changed metadata");
    QCOMPARE(model.observeInventory(frame(3, {output(), second})).status,
             InventoryObservationStatus::AcceptedChanged);
    QCOMPARE(model.snapshot()->outputs.at(1).model,
             QStringLiteral("Changed metadata"));
    QCOMPARE(model.observeInventory(frame(4, {second})).status,
             InventoryObservationStatus::AcceptedChanged);
    QCOMPARE(model.snapshot()->outputs.size(), 1);
    QCOMPARE(model.snapshot()->outputs.constFirst().stableId,
             QStringLiteral("conn:HDMI-A-1"));
}

void DisplayServiceModelTest::ownsPreviewConfirmAndRevertTransitions()
{
    FakeClock clock;
    FakeTransactionPort port;
    DisplayServiceModel model(clock, port, [] { return QStringLiteral("epoch-a"); });
    QVERIFY(model.observeInventory(frame(1, {output()})).accepted());
    QVERIFY(model.safetyChanged(DisplayTransaction::SafetyState::Safe).accepted);

    Display::Candidate rotated =
        DisplayTopology::candidateFromSnapshot(*model.snapshot());
    rotated.outputs[0].transform = Display::Transform::Rotate180;
    const ServiceOperationResult staged = model.stage(QStringLiteral("tx-1"), rotated);
    QVERIFY(staged.command.accepted);
    QCOMPARE(staged.operation.status, Display::OperationStatus::Accepted);
    InventoryFrame equalGenerationCollision = frame(1, {output()});
    equalGenerationCollision.outputs[0].model = QStringLiteral("collision");
    QCOMPARE(model.observeInventory(equalGenerationCollision).reasonCode,
             QStringLiteral("changed-equal-output-generation"));
    QCOMPARE(model.view()->state, DisplayTransaction::MachineState::Staged);
    QVERIFY(port.storedJournals.isEmpty());
    QVERIFY(model.preview(QStringLiteral("tx-1")).command.accepted);
    QCOMPARE(port.storedJournals.size(), 1);
    QCOMPARE(port.applyRequests.size(), 1);
    QCOMPARE(port.applyRequests.constLast().scope,
             DisplayTransaction::ApplyScope::ForwardCandidate);

    QVERIFY(model.applyCompleted(model.machineLineage(),
                                 port.applyRequests.constLast().token,
                                 DisplayTransaction::ApplyOutcome::Applied)
                .accepted);
    QVERIFY(model.observeInventory(frame(
                                      2,
                                      {output(QStringLiteral("DP-1"),
                                              QRect(0, 0, 1920, 1080), 1.0,
                                              Display::Transform::Rotate180)}))
                .accepted());
    QCOMPARE(model.view()->state,
             DisplayTransaction::MachineState::AwaitingConfirmation);
    const ServiceOperationResult confirmed = model.confirm(QStringLiteral("tx-1"));
    QVERIFY(confirmed.command.accepted);
    QCOMPARE(confirmed.operation.status, Display::OperationStatus::Succeeded);
    QCOMPARE(model.view()->state, DisplayTransaction::MachineState::Ready);

    Display::Candidate normal =
        DisplayTopology::candidateFromSnapshot(*model.snapshot());
    normal.outputs[0].transform = Display::Transform::Normal;
    QVERIFY(model.stage(QStringLiteral("tx-2"), normal).command.accepted);
    QVERIFY(model.preview(QStringLiteral("tx-2")).command.accepted);
    QVERIFY(model.applyCompleted(model.machineLineage(),
                                 port.applyRequests.constLast().token,
                                 DisplayTransaction::ApplyOutcome::Applied)
                .accepted);
    QVERIFY(model.observeInventory(frame(3, {output()})).accepted());
    QCOMPARE(model.view()->state,
             DisplayTransaction::MachineState::AwaitingConfirmation);
    QVERIFY(model.cancel(QStringLiteral("tx-2")).command.accepted);
    QCOMPARE(port.applyRequests.constLast().scope,
             DisplayTransaction::ApplyScope::FullPreimage);
    QCOMPARE(model.view()->state, DisplayTransaction::MachineState::RevertingApply);
}

void DisplayServiceModelTest::fencesLateCompletionAcrossMachineReplacement()
{
    FakeClock clock;
    FakeTransactionPort port;
    QStringList epochs{QStringLiteral("epoch-a"), QStringLiteral("epoch-b")};
    DisplayServiceModel model(clock, port, [&epochs] { return epochs.takeFirst(); });

    QVERIFY(model.observeInventory(frame(1, {output()})).accepted());
    QVERIFY(model.safetyChanged(DisplayTransaction::SafetyState::Safe).accepted);
    Display::Candidate first =
        DisplayTopology::candidateFromSnapshot(*model.snapshot());
    first.outputs[0].transform = Display::Transform::Rotate180;
    QVERIFY(model.stage(QStringLiteral("first"), first).command.accepted);
    QVERIFY(model.preview(QStringLiteral("first")).command.accepted);
    const quint64 oldLineage = port.requestMachineLineages.constLast();
    const quint64 reusedToken = port.applyRequests.constLast().token;

    QVERIFY(model.transportLost());
    QVERIFY(model.observeInventory(
                     frame(1, {output()}, QStringLiteral(":1.77")))
                .accepted());
    QVERIFY(model.safetyChanged(DisplayTransaction::SafetyState::Safe).accepted);
    Display::Candidate second =
        DisplayTopology::candidateFromSnapshot(*model.snapshot());
    second.outputs[0].transform = Display::Transform::Rotate90;
    QVERIFY(model.stage(QStringLiteral("second"), second).command.accepted);
    QVERIFY(model.preview(QStringLiteral("second")).command.accepted);
    QCOMPARE(port.applyRequests.constLast().token, reusedToken);
    QVERIFY(port.requestMachineLineages.constLast() != oldLineage);

    const DisplayTransaction::CommandResult stale = model.applyCompleted(
        oldLineage, reusedToken, DisplayTransaction::ApplyOutcome::Applied);
    QVERIFY(!stale.accepted);
    QCOMPARE(stale.error, DisplayTransaction::CommandError::CallbackOutOfOrder);
    QCOMPARE(model.view()->state, DisplayTransaction::MachineState::Applying);
    QVERIFY(model.applyCompleted(port.requestMachineLineages.constLast(),
                                 reusedToken,
                                 DisplayTransaction::ApplyOutcome::Applied)
                .accepted);
}

void DisplayServiceModelTest::projectsZeroOrOneValidatedTransactionSummary()
{
    FakeClock clock;
    FakeTransactionPort port;
    DisplayServiceModel model(clock, port, [] { return QStringLiteral("epoch-a"); });
    QVERIFY(model.observeInventory(frame(1, {output()})).accepted());
    QVERIFY(model.safetyChanged(DisplayTransaction::SafetyState::Safe).accepted);

    const auto validatedWithNoSummary = [&model]() {
        QVERIFY(Display::validateSnapshot(*model.snapshot()).accepted);
        QCOMPARE(model.snapshot()->transactions.size(), qsizetype(0));
    };
    const auto validatedWithOneSummary = [&model](Display::TransactionSummary &out) {
        QVERIFY(Display::validateSnapshot(*model.snapshot()).accepted);
        QCOMPARE(model.snapshot()->transactions.size(), qsizetype(1));
        out = model.snapshot()->transactions.constFirst();
    };

    Display::TransactionSummary summary;

    // None: Ready holds no active transaction.
    validatedWithNoSummary();

    Display::Candidate rotated =
        DisplayTopology::candidateFromSnapshot(*model.snapshot());
    rotated.outputs[0].transform = Display::Transform::Rotate180;
    const QString epoch = model.snapshot()->serviceEpoch;
    const QByteArray fingerprint = model.snapshot()->liveFingerprint;
    const QList<Display::Output> outputs = model.snapshot()->outputs;
    QVERIFY(model.stage(QStringLiteral("tx-1"), rotated).command.accepted);

    // Staged: stage-time lineage, no reason, no deadline, and the projection
    // leaves epoch/fingerprint/outputs byte-identical to the machine snapshot.
    validatedWithOneSummary(summary);
    QCOMPARE(summary,
             (Display::TransactionSummary{
                 .transactionId = QStringLiteral("tx-1"),
                 .state = Display::TransactionState::Staged,
                 .reason = Display::TransactionReason::None,
                 .initiatingEpoch = epoch,
                 .baseRevision = 1,
                 .observedRevision = 1,
                 .deadlineMonotonicMilliseconds = 0,
                 .revertAttempt = 0}));
    QCOMPARE(model.snapshot()->serviceEpoch, epoch);
    QCOMPARE(model.snapshot()->liveFingerprint, fingerprint);
    QCOMPARE(model.snapshot()->outputs, outputs);

    // Preview moves to Applying under the apply deadline.
    QVERIFY(model.preview(QStringLiteral("tx-1")).command.accepted);
    validatedWithOneSummary(summary);
    QCOMPARE(summary,
             (Display::TransactionSummary{
                 .transactionId = QStringLiteral("tx-1"),
                 .state = Display::TransactionState::Applying,
                 .reason = Display::TransactionReason::None,
                 .initiatingEpoch = epoch,
                 .baseRevision = 1,
                 .observedRevision = 1,
                 .deadlineMonotonicMilliseconds = 5'100,
                 .revertAttempt = 0}));

    // Apply completion enters Observing under the observation deadline.
    QVERIFY(model.applyCompleted(model.machineLineage(),
                                port.applyRequests.constLast().token,
                                DisplayTransaction::ApplyOutcome::Applied)
                .accepted);
    validatedWithOneSummary(summary);
    QCOMPARE(summary,
             (Display::TransactionSummary{
                 .transactionId = QStringLiteral("tx-1"),
                 .state = Display::TransactionState::Observing,
                 .reason = Display::TransactionReason::None,
                 .initiatingEpoch = epoch,
                 .baseRevision = 1,
                 .observedRevision = 1,
                 .deadlineMonotonicMilliseconds = 2'100,
                 .revertAttempt = 0}));

    // Observed target truth enters AwaitingConfirmation; the summary keeps
    // the stage-time base revision beside the newly observed revision.
    QVERIFY(model.observeInventory(frame(
                                   2,
                                   {output(QStringLiteral("DP-1"),
                                           QRect(0, 0, 1920, 1080), 1.0,
                                           Display::Transform::Rotate180)}))
                .accepted());
    validatedWithOneSummary(summary);
    QCOMPARE(summary,
             (Display::TransactionSummary{
                 .transactionId = QStringLiteral("tx-1"),
                 .state = Display::TransactionState::AwaitingConfirmation,
                 .reason = Display::TransactionReason::None,
                 .initiatingEpoch = epoch,
                 .baseRevision = 1,
                 .observedRevision = 2,
                 .deadlineMonotonicMilliseconds = 15'100,
                 .revertAttempt = 0}));

    // Terminal: confirmation completes the transaction and the public
    // snapshot returns to exactly zero summaries.
    QVERIFY(model.confirm(QStringLiteral("tx-1")).command.accepted);
    QCOMPARE(model.view()->state, DisplayTransaction::MachineState::Ready);
    validatedWithNoSummary();
    QCOMPARE(model.snapshot()->revision, quint64(2));

}

void DisplayServiceModelTest::projectsRevertAndStuckSummaries()
{
    FakeClock clock;
    FakeTransactionPort port;
    DisplayServiceModel model(clock, port, [] { return QStringLiteral("epoch-a"); });
    QVERIFY(model.observeInventory(frame(1, {output()})).accepted());
    QVERIFY(model.safetyChanged(DisplayTransaction::SafetyState::Safe).accepted);
    Display::TransactionSummary summary;
    const auto validatedWithOneSummary = [&model](Display::TransactionSummary &out) {
        QVERIFY(Display::validateSnapshot(*model.snapshot()).accepted);
        QCOMPARE(model.snapshot()->transactions.size(), qsizetype(1));
        out = model.snapshot()->transactions.constFirst();
    };

    // Revert: a transaction rolled back to exhaustion publishes the bounded
    // rollback lineage through Stuck.
    Display::Candidate rotated =
        DisplayTopology::candidateFromSnapshot(*model.snapshot());
    rotated.outputs[0].transform = Display::Transform::Rotate180;
    const QString epoch = model.snapshot()->serviceEpoch;
    QVERIFY(model.stage(QStringLiteral("tx-2"), rotated).command.accepted);
    QVERIFY(model.preview(QStringLiteral("tx-2")).command.accepted);
    QVERIFY(model.applyCompleted(model.machineLineage(),
                                port.applyRequests.constLast().token,
                                DisplayTransaction::ApplyOutcome::Applied)
                .accepted);
    QVERIFY(model.observeInventory(frame(
                                   2,
                                   {output(QStringLiteral("DP-1"),
                                           QRect(0, 0, 1920, 1080), 1.0,
                                           Display::Transform::Rotate180)}))
                .accepted());
    validatedWithOneSummary(summary);
    QCOMPARE(summary,
             (Display::TransactionSummary{
                 .transactionId = QStringLiteral("tx-2"),
                 .state = Display::TransactionState::AwaitingConfirmation,
                 .reason = Display::TransactionReason::None,
                 .initiatingEpoch = epoch,
                 .baseRevision = 1,
                 .observedRevision = 2,
                 .deadlineMonotonicMilliseconds = 15'100,
                 .revertAttempt = 0}));

    QVERIFY(model.cancel(QStringLiteral("tx-2")).command.accepted);
    validatedWithOneSummary(summary);
    QCOMPARE(summary,
             (Display::TransactionSummary{
                 .transactionId = QStringLiteral("tx-2"),
                 .state = Display::TransactionState::Reverting,
                 .reason = Display::TransactionReason::Cancelled,
                 .initiatingEpoch = epoch,
                 .baseRevision = 1,
                 .observedRevision = 2,
                 .deadlineMonotonicMilliseconds = 5'100,
                 .revertAttempt = 1}));

    QVERIFY(model.applyCompleted(model.machineLineage(),
                                port.applyRequests.constLast().token,
                                DisplayTransaction::ApplyOutcome::Rejected)
                .accepted);
    clock.now = 400;
    QVERIFY(model.tick().accepted);
    validatedWithOneSummary(summary);
    QCOMPARE(summary,
             (Display::TransactionSummary{
                 .transactionId = QStringLiteral("tx-2"),
                 .state = Display::TransactionState::Reverting,
                 .reason = Display::TransactionReason::Cancelled,
                 .initiatingEpoch = epoch,
                 .baseRevision = 1,
                 .observedRevision = 2,
                 .deadlineMonotonicMilliseconds = 5'400,
                 .revertAttempt = 2}));

    QVERIFY(model.applyCompleted(model.machineLineage(),
                                port.applyRequests.constLast().token,
                                DisplayTransaction::ApplyOutcome::Rejected)
                .accepted);
    clock.now = 1'000;
    QVERIFY(model.tick().accepted);
    validatedWithOneSummary(summary);
    QCOMPARE(summary,
             (Display::TransactionSummary{
                 .transactionId = QStringLiteral("tx-2"),
                 .state = Display::TransactionState::Reverting,
                 .reason = Display::TransactionReason::Cancelled,
                 .initiatingEpoch = epoch,
                 .baseRevision = 1,
                 .observedRevision = 2,
                 .deadlineMonotonicMilliseconds = 6'000,
                 .revertAttempt = 3}));

    QVERIFY(model.applyCompleted(model.machineLineage(),
                                port.applyRequests.constLast().token,
                                DisplayTransaction::ApplyOutcome::Rejected)
                .accepted);
    QCOMPARE(model.view()->state, DisplayTransaction::MachineState::Stuck);
    validatedWithOneSummary(summary);
    QCOMPARE(summary,
             (Display::TransactionSummary{
                 .transactionId = QStringLiteral("tx-2"),
                 .state = Display::TransactionState::Stuck,
                 .reason = Display::TransactionReason::RevertFailed,
                 .initiatingEpoch = epoch,
                 .baseRevision = 1,
                 .observedRevision = 2,
                 .deadlineMonotonicMilliseconds = 0,
                 .revertAttempt = 3}));

    // Transport loss withdraws the whole public snapshot.
    QVERIFY(model.transportLost());
    QVERIFY(model.snapshot() == nullptr);
}

void DisplayServiceModelTest::projectionFailsClosedOnInvalidView()
{
    FakeClock clock;
    FakeTransactionPort port;
    DisplayServiceModel model(clock, port, [] { return QStringLiteral("epoch-a"); });
    QVERIFY(model.observeInventory(frame(1, {output()})).accepted());

    const Display::Snapshot snapshot = *model.snapshot();
    DisplayTransaction::MachineView view;
    DisplayTransaction::Journal journal;
    journal.target = DisplayTopology::candidateFromSnapshot(snapshot);

    // Discovering and Ready carry no active transaction.
    view.state = DisplayTransaction::MachineState::Discovering;
    view.transactionId = QStringLiteral("tx-1");
    QVERIFY(!Private::projectedTransactionSummary(snapshot, view, journal)
                 .has_value());
    view.state = DisplayTransaction::MachineState::Ready;
    QVERIFY(!Private::projectedTransactionSummary(snapshot, view, journal)
                 .has_value());

    // A valid active view projects the mapped state with exact lineage.
    view.state = DisplayTransaction::MachineState::RevertBackoff;
    view.currentRevision = snapshot.revision;
    const std::optional<Display::TransactionSummary> projected =
        Private::projectedTransactionSummary(snapshot, view, journal);
    QVERIFY(projected.has_value());
    QCOMPARE(projected->state, Display::TransactionState::Reverting);
    QCOMPARE(projected->baseRevision, snapshot.revision);
    QCOMPARE(projected->initiatingEpoch, snapshot.serviceEpoch);

    // Every incomplete, unbounded, or lineage-divergent input fails closed.
    view.state = DisplayTransaction::MachineState::Applying;
    view.currentRevision = snapshot.revision;

    view.transactionId.clear();
    QVERIFY(!Private::projectedTransactionSummary(snapshot, view, journal)
                 .has_value());
    view.transactionId = QStringLiteral("tx-1");

    journal.target.baseRevision = 0;
    QVERIFY(!Private::projectedTransactionSummary(snapshot, view, journal)
                 .has_value());
    journal.target.baseRevision = snapshot.revision;

    view.currentRevision = snapshot.revision + 1;
    QVERIFY(!Private::projectedTransactionSummary(snapshot, view, journal)
                 .has_value());
    view.currentRevision = snapshot.revision;

    view.revertAttempt = Display::kMaximumRevertAttempts + 1;
    QVERIFY(!Private::projectedTransactionSummary(snapshot, view, journal)
                 .has_value());
    view.revertAttempt = 0;

    Display::Snapshot epochless = snapshot;
    epochless.serviceEpoch.clear();
    QVERIFY(!Private::projectedTransactionSummary(epochless, view, journal)
                 .has_value());
}

QTEST_MAIN(DisplayServiceModelTest)

#include "tst_display_service_model.moc"
