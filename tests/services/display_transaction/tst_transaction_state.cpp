// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/transaction_test_support.h"

#include <QtTest>

using namespace QindaQt::DisplayTransaction;
namespace Display = QindaQt::Display;
namespace DisplayTopology = QindaQt::DisplayTopology;

namespace
{

enum class ReadySnapshotInput {
    Observation,
    ExternalIntent,
    Topology,
};

CommandResult deliverReadySnapshot(Machine &machine,
                                   const ReadySnapshotInput input,
                                   const Display::Snapshot &snapshot)
{
    switch (input) {
    case ReadySnapshotInput::Observation:
        return machine.observedSnapshot(snapshot);
    case ReadySnapshotInput::ExternalIntent:
        return machine.externalIntentObserved(snapshot);
    case ReadySnapshotInput::Topology:
        return machine.topologyChanged(snapshot);
    }
    Q_UNREACHABLE_RETURN({});
}

} // namespace

class TransactionStateTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stageFencesRevisionAndDetectsNoOp();
    void previewRequiresSafeAuthorityAndDurableJournal();
    void journalDurabilityUncertaintyIsConservative();
    void applyObserveConfirmFlow();
    void readyInputsEnforceCurrentLineage();
    void rejectedAndTimedOutApplyNeverReplayForward();
    void acknowledgedNoOpRollbackCompletesFromRetainedPreimage();
    void acknowledgedRollbackWithNonPreimageRetainedSnapshotEntersObservation();
    void observationMismatchTimeoutAndInvalidCallbacks();
    void inventoryObservationBeforeApplyAckReachesAwaitingConfirmation();
    void inventoryObservationBeforeApplyAckMismatchStaysObserving();
    void inventoryObservationBeforeApplyAckMatchingPreimageIsNotRejection();
    void staleOrWrongLineageObservationDuringApplyIsRejected();
    void inventoryObservationBeforeRevertAckCompletesNoOpRollback();
};

void TransactionStateTests::stageFencesRevisionAndDetectsNoOp()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);

    Display::Candidate stale = Test::changedCandidate(base);
    stale.baseRevision++;
    const MachineView before = machine.view();
    QCOMPARE(machine.stage(QStringLiteral("tx"), stale).error, CommandError::StaleRevision);
    QCOMPARE(machine.view(), before);

    const CommandResult noOp = machine.stage(
        QStringLiteral("tx"), DisplayTopology::candidateFromSnapshot(base));
    QVERIFY(noOp.accepted);
    QCOMPARE(noOp.error, CommandError::NoOp);
    QCOMPARE(machine.view(), before);
    QVERIFY(port.requests.isEmpty());

    QVERIFY(machine.stage(QStringLiteral("tx"), Test::changedCandidate(base)).accepted);
    QCOMPARE(machine.view().state, MachineState::Staged);
    const MachineView staged = machine.view();
    QCOMPARE(machine.stage(QStringLiteral("other"), Test::changedCandidate(base)).error,
             CommandError::TransactionActive);
    QCOMPARE(machine.view(), staged);
}

void TransactionStateTests::readyInputsEnforceCurrentLineage()
{
    const Display::Snapshot base = Test::snapshot(false, 2);
    for (const ReadySnapshotInput input : {
             ReadySnapshotInput::Observation,
             ReadySnapshotInput::ExternalIntent,
             ReadySnapshotInput::Topology,
         }) {
        Test::FakeClock clock;
        Test::FakePort port;
        Machine machine(clock, port, Test::timing());
        QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
        const MachineView beforeView = machine.view();
        const Display::Snapshot beforeSnapshot = machine.currentSnapshot();

        const CommandResult unchangedResult = deliverReadySnapshot(machine, input, base);
        QVERIFY(unchangedResult.accepted);
        QVERIFY(!unchangedResult.stateChanged);
        QCOMPARE(machine.view(), beforeView);
        QCOMPARE(machine.currentSnapshot(), beforeSnapshot);

        const Display::Candidate changedCandidate = Test::changedCandidate(base);
        const Display::Snapshot changedAtSameRevision =
            Test::observed(base, changedCandidate, base.revision);
        QVERIFY(changedAtSameRevision != base);
        const CommandResult changedAtSameRevisionResult = deliverReadySnapshot(
            machine, input, changedAtSameRevision);
        QVERIFY(!changedAtSameRevisionResult.accepted);
        QVERIFY(!changedAtSameRevisionResult.stateChanged);
        QCOMPARE(changedAtSameRevisionResult.error, CommandError::InvalidSnapshot);
        QCOMPARE(machine.view(), beforeView);
        QCOMPARE(machine.currentSnapshot(), beforeSnapshot);
        QCOMPARE(port.storeCalls, 0);
        QCOMPARE(port.clearCalls, 0);
        QVERIFY(port.requests.isEmpty());

        Display::Snapshot older = base;
        older.revision--;
        const CommandResult olderResult = deliverReadySnapshot(machine, input, older);
        QCOMPARE(olderResult.error, CommandError::InvalidSnapshot);
        QCOMPARE(machine.view(), beforeView);
        QCOMPARE(machine.currentSnapshot(), beforeSnapshot);

        Display::Snapshot otherEpoch = base;
        otherEpoch.serviceEpoch = QStringLiteral("other-epoch");
        otherEpoch.revision++;
        const CommandResult otherEpochResult = deliverReadySnapshot(
            machine, input, otherEpoch);
        QCOMPARE(otherEpochResult.error, CommandError::InvalidSnapshot);
        QCOMPARE(machine.view(), beforeView);
        QCOMPARE(machine.currentSnapshot(), beforeSnapshot);

        Display::Snapshot newer = changedAtSameRevision;
        newer.revision++;
        const CommandResult newerResult = deliverReadySnapshot(machine, input, newer);
        QVERIFY(newerResult.accepted);
        QVERIFY(newerResult.stateChanged);
        QCOMPARE(machine.currentSnapshot(), newer);
        QCOMPARE(machine.currentSnapshot().revision, newer.revision);

        const Display::Candidate preChangeCandidate =
            DisplayTopology::candidateFromSnapshot(base);
        QCOMPARE(machine.stage(QStringLiteral("stale-pre-change"), preChangeCandidate).error,
                 CommandError::StaleRevision);
        QCOMPARE(machine.currentSnapshot(), newer);
        QCOMPARE(machine.view().state, MachineState::Ready);
    }
}

void TransactionStateTests::previewRequiresSafeAuthorityAndDurableJournal()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    QVERIFY(machine.initialize(base, SafetyState::Unknown).accepted);
    QVERIFY(machine.stage(QStringLiteral("tx"), Test::changedCandidate(base)).accepted);
    const MachineView staged = machine.view();
    QCOMPARE(machine.preview(QStringLiteral("tx")).error, CommandError::Locked);
    QCOMPARE(machine.view(), staged);
    QVERIFY(machine.safetyChanged(SafetyState::Safe).accepted);
    port.storeSucceeds = false;
    const MachineView safeStaged = machine.view();
    QCOMPARE(machine.preview(QStringLiteral("tx")).error, CommandError::JournalFailure);
    QCOMPARE(machine.view(), safeStaged);
    QVERIFY(!port.journalPresent);
    QVERIFY(port.requests.isEmpty());

    port.storeSucceeds = true;
    QVERIFY(machine.preview(QStringLiteral("tx")).accepted);
    QCOMPARE(machine.view().state, MachineState::Applying);
    QVERIFY(machine.view().journalActive);
    QCOMPARE(port.requests.size(), 1);
    QCOMPARE(port.requests.first().scope, ApplyScope::ForwardCandidate);
}

void TransactionStateTests::journalDurabilityUncertaintyIsConservative()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    QVERIFY(machine.stage(QStringLiteral("tx"), candidate).accepted);

    port.storeOutcome = JournalMutationOutcome::DurabilityUncertain;
    const CommandResult uncertainPreview = machine.preview(QStringLiteral("tx"));
    QVERIFY(uncertainPreview.accepted);
    QVERIFY(uncertainPreview.stateChanged);
    QCOMPARE(uncertainPreview.error, CommandError::JournalFailure);
    QCOMPARE(machine.view().state, MachineState::Stuck);
    QCOMPARE(machine.view().reason, Display::TransactionReason::JournalFailure);
    QCOMPARE(machine.view().transactionId, QStringLiteral("tx"));
    QVERIFY(machine.view().journalActive);
    QCOMPARE(machine.activeJournal().phase, JournalPhase::Applying);
    QCOMPARE(machine.activeJournal(), port.journal);
    QVERIFY(port.requests.isEmpty());
    QVERIFY(port.journalPresent);
    QCOMPARE(port.storeCalls, 1);

    const MachineView cleanup = machine.view();
    const Journal retained = machine.activeJournal();
    QCOMPARE(machine.cancel(QStringLiteral("tx")).error, CommandError::InvalidTransition);
    QCOMPARE(machine.stage(QStringLiteral("replacement"), candidate).error,
             CommandError::TransactionActive);
    QCOMPARE(machine.preview(QStringLiteral("tx")).error, CommandError::InvalidTransition);
    QCOMPARE(machine.view(), cleanup);
    QCOMPARE(machine.activeJournal(), retained);
    QCOMPARE(port.storeCalls, 1);
    QCOMPARE(port.clearCalls, 0);
    QVERIFY(port.journalPresent);

    port.clearSucceeds = false;
    const CommandResult unchangedClear = machine.retryStuck();
    QVERIFY(unchangedClear.accepted);
    QVERIFY(!unchangedClear.stateChanged);
    QCOMPARE(unchangedClear.error, CommandError::JournalFailure);
    QCOMPARE(machine.view(), cleanup);
    QCOMPARE(machine.activeJournal(), retained);
    QVERIFY(port.journalPresent);

    port.clearSucceeds = true;
    port.clearOutcome = JournalMutationOutcome::DurabilityUncertain;
    const CommandResult uncertainClear = machine.retryStuck();
    QVERIFY(uncertainClear.accepted);
    QVERIFY(!uncertainClear.stateChanged);
    QCOMPARE(uncertainClear.error, CommandError::JournalFailure);
    QCOMPARE(machine.view(), cleanup);
    QCOMPARE(machine.activeJournal(), retained);
    QVERIFY(!port.journalPresent);
    QCOMPARE(machine.cancel(QStringLiteral("tx")).error, CommandError::InvalidTransition);
    QCOMPARE(machine.view(), cleanup);

    port.clearOutcome = JournalMutationOutcome::Durable;
    const CommandResult durableClear = machine.retryStuck();
    QVERIFY(durableClear.accepted);
    QVERIFY(durableClear.stateChanged);
    QCOMPARE(machine.view().state, MachineState::Ready);
    QVERIFY(!machine.view().journalActive);
    QCOMPARE(machine.view().lastTerminalReason,
             Display::TransactionReason::JournalFailure);
    QVERIFY(!port.journalPresent);
    QCOMPARE(port.clearCalls, 3);
    QVERIFY(port.requests.isEmpty());
}

void TransactionStateTests::applyObserveConfirmFlow()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(machine, port, base, candidate);
    QCOMPARE(machine.view().deadlineMonotonicMilliseconds,
             clock.now + Test::timing().confirmationTimeoutMilliseconds);
    QCOMPARE(port.journal.phase, JournalPhase::AwaitingConfirmation);
    QVERIFY(machine.confirm(QStringLiteral("tx")).accepted);
    QCOMPARE(machine.view().state, MachineState::Ready);
    QCOMPARE(machine.currentSnapshot().revision, quint64(2));
    QVERIFY(!port.journalPresent);
}

void TransactionStateTests::rejectedAndTimedOutApplyNeverReplayForward()
{
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    {
        Test::FakeClock clock;
        Test::FakePort port;
        Machine machine(clock, port, Test::timing());
        QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
        QVERIFY(machine.stage(QStringLiteral("tx"), candidate).accepted);
        QVERIFY(machine.preview(QStringLiteral("tx")).accepted);
        const quint64 token = port.requests.first().token;
        QCOMPARE(machine.applyCompleted(token, ApplyOutcome::Rejected).error,
                 CommandError::ApplyRejected);
        QCOMPARE(machine.view().state, MachineState::ResolvingUncertain);
        QVERIFY(machine.observedSnapshot(base).accepted);
        QCOMPARE(machine.view().state, MachineState::Ready);
        QCOMPARE(machine.view().lastTerminalReason,
                 Display::TransactionReason::ApplyRejected);
        QCOMPARE(port.requests.size(), 1);
    }
    {
        Test::FakeClock clock;
        Test::FakePort port;
        Machine machine(clock, port, Test::timing());
        QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
        QVERIFY(machine.stage(QStringLiteral("tx"), candidate).accepted);
        QVERIFY(machine.preview(QStringLiteral("tx")).accepted);
        clock.advance(Test::timing().applyTimeoutMilliseconds);
        QCOMPARE(machine.tick().error, CommandError::ApplyUncertain);
        QCOMPARE(machine.view().state, MachineState::ResolvingUncertain);
        QCOMPARE(port.requests.size(), 1);
        clock.advance(Test::timing().observationTimeoutMilliseconds);
        QVERIFY(machine.tick().accepted);
        QCOMPARE(machine.view().state, MachineState::RevertingApply);
        QCOMPARE(port.requests.size(), 2);
        QCOMPARE(port.requests.last().scope, ApplyScope::FullPreimage);
        QCOMPARE(std::count_if(port.requests.cbegin(), port.requests.cend(),
                               [](const ApplyRequest &request) {
                                   return request.scope == ApplyScope::ForwardCandidate;
                               }),
                 1);
    }
}

void TransactionStateTests::acknowledgedNoOpRollbackCompletesFromRetainedPreimage()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToObserving(machine, port, candidate);

    clock.advance(Test::timing().observationTimeoutMilliseconds);
    QCOMPARE(machine.tick().error, CommandError::ObservationTimeout);
    QCOMPARE(machine.view().state, MachineState::RevertingApply);
    const quint64 rollbackToken = port.requests.last().token;

    QVERIFY(machine.applyCompleted(rollbackToken, ApplyOutcome::Applied).accepted);
    QCOMPARE(machine.view().state, MachineState::Ready);
    QCOMPARE(machine.currentSnapshot(), base);
    QCOMPARE(machine.view().lastTerminalReason,
             Display::TransactionReason::ObservationTimeout);
    QVERIFY(!port.journalPresent);
    QCOMPARE(port.requests.size(), 2);
}

void TransactionStateTests::acknowledgedRollbackWithNonPreimageRetainedSnapshotEntersObservation()
{
    // Counterexample to acknowledgedNoOpRollbackCompletesFromRetainedPreimage:
    // when the retained snapshot at the Applied ack is neither the preimage
    // nor the target, the no-op short-circuit must not fire and the machine
    // must still prove restoration through RevertingObserve.
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToObserving(machine, port, candidate);

    Display::Candidate mismatchCandidate = candidate;
    mismatchCandidate.outputs[0].modeId = QStringLiteral("small");
    const Display::Snapshot mismatch = Test::observed(base, mismatchCandidate, 2);
    QVERIFY(machine.observedSnapshot(mismatch).accepted);
    QCOMPARE(machine.view().state, MachineState::Observing);

    clock.advance(Test::timing().observationTimeoutMilliseconds);
    QCOMPARE(machine.tick().error, CommandError::ObservationTimeout);
    QCOMPARE(machine.view().state, MachineState::RevertingApply);
    const quint64 rollbackToken = port.requests.last().token;

    QVERIFY(machine.applyCompleted(rollbackToken, ApplyOutcome::Applied).accepted);
    QCOMPARE(machine.view().state, MachineState::RevertingObserve);
    QVERIFY(port.journalPresent);
    QCOMPARE(port.requests.size(), 2);
}

void TransactionStateTests::observationMismatchTimeoutAndInvalidCallbacks()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    const MachineView ready = machine.view();
    QCOMPARE(machine.applyCompleted(99, ApplyOutcome::Applied).error,
             CommandError::CallbackOutOfOrder);
    QCOMPARE(machine.view(), ready);

    Test::previewToObserving(machine, port, candidate);
    const MachineView observing = machine.view();
    QCOMPARE(machine.applyCompleted(port.requests.first().token, ApplyOutcome::Applied).error,
             CommandError::CallbackOutOfOrder);
    QCOMPARE(machine.view(), observing);

    Display::Candidate mismatchCandidate = candidate;
    mismatchCandidate.outputs[0].modeId = QStringLiteral("small");
    const Display::Snapshot mismatch = Test::observed(base, mismatchCandidate, 2);
    const CommandResult mismatchResult = machine.observedSnapshot(mismatch);
    QVERIFY(mismatchResult.accepted);
    QCOMPARE(mismatchResult.error, CommandError::ObservationMismatch);
    QCOMPARE(machine.view().state, MachineState::Observing);
    clock.advance(Test::timing().observationTimeoutMilliseconds);
    QCOMPARE(machine.tick().error, CommandError::ObservationTimeout);
    QCOMPARE(machine.view().state, MachineState::RevertingApply);
    QCOMPARE(port.requests.last().scope, ApplyScope::FullPreimage);
}

void TransactionStateTests::inventoryObservationBeforeApplyAckReachesAwaitingConfirmation()
{
    // The independent D0 inventory update and the Wayland Applied callback are
    // delivered on separate channels; inventory can win the race and land
    // while the machine is still Applying. That valid, same-lineage
    // observation must be retained and evaluated as soon as the matching ack
    // arrives, reaching AwaitingConfirmation without a second inventory event.
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    QVERIFY(machine.stage(QStringLiteral("tx"), candidate).accepted);
    QVERIFY(machine.preview(QStringLiteral("tx")).accepted);
    QCOMPARE(machine.view().state, MachineState::Applying);
    const quint64 token = port.requests.last().token;

    const Display::Snapshot targetObserved = Test::observed(base, candidate, base.revision + 1);
    const CommandResult observation = machine.observedSnapshot(targetObserved);
    QVERIFY(observation.accepted);
    QCOMPARE(observation.error, CommandError::None);
    QCOMPARE(machine.view().state, MachineState::Applying);
    QCOMPARE(machine.currentSnapshot(), targetObserved);

    const CommandResult ackResult = machine.applyCompleted(token, ApplyOutcome::Applied);
    QVERIFY(ackResult.accepted);
    QCOMPARE(machine.view().state, MachineState::AwaitingConfirmation);
    QCOMPARE(port.journal.phase, JournalPhase::AwaitingConfirmation);
    QCOMPARE(machine.view().deadlineMonotonicMilliseconds,
             clock.now + Test::timing().confirmationTimeoutMilliseconds);
    QCOMPARE(port.requests.size(), 1);
}

void TransactionStateTests::inventoryObservationBeforeApplyAckMismatchStaysObserving()
{
    // A retained pre-ack observation that does not match the staged target
    // must never be evaluated at ack time; only a target-matching retained
    // snapshot may short-circuit past Observing. A non-matching retained
    // snapshot (here, one that also happens to equal neither the target nor
    // the pre-image) must land in plain Observing and wait for a genuinely
    // post-ack observation or the observation timeout, not be judged here.
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    QVERIFY(machine.stage(QStringLiteral("tx"), candidate).accepted);
    QVERIFY(machine.preview(QStringLiteral("tx")).accepted);
    const quint64 token = port.requests.last().token;

    Display::Candidate mismatchCandidate = candidate;
    mismatchCandidate.outputs[0].modeId = QStringLiteral("small");
    const Display::Snapshot mismatch = Test::observed(base, mismatchCandidate, base.revision + 1);
    const CommandResult observation = machine.observedSnapshot(mismatch);
    QVERIFY(observation.accepted);
    QCOMPARE(machine.view().state, MachineState::Applying);

    const CommandResult ackResult = machine.applyCompleted(token, ApplyOutcome::Applied);
    QVERIFY(ackResult.accepted);
    QCOMPARE(ackResult.error, CommandError::None);
    QCOMPARE(machine.view().state, MachineState::Observing);
    QCOMPARE(machine.currentSnapshot(), mismatch);
    QVERIFY(port.journalPresent);
    QCOMPARE(machine.view().deadlineMonotonicMilliseconds,
             clock.now + Test::timing().observationTimeoutMilliseconds);
}

void TransactionStateTests::inventoryObservationBeforeApplyAckMatchingPreimageIsNotRejection()
{
    // Counterexample to inventoryObservationBeforeApplyAckReachesAwaitingConfirmation:
    // an advanced, same-lineage observation retained during Applying can
    // coincidentally equal the pre-image (for example descriptive-only
    // inventory metadata observed before the compositor has actually applied
    // the candidate). That must never be read as proof the apply was
    // rejected at ack time; the retained truth is kept, but the transaction
    // simply proceeds to ordinary Observing and is proven or timed out by a
    // later, genuinely post-ack observation.
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    QVERIFY(machine.stage(QStringLiteral("tx"), candidate).accepted);
    QVERIFY(machine.preview(QStringLiteral("tx")).accepted);
    const quint64 token = port.requests.last().token;

    const Display::Snapshot preimageObserved = Test::observed(
        base, DisplayTopology::candidateFromSnapshot(base), base.revision + 1);
    const CommandResult observation = machine.observedSnapshot(preimageObserved);
    QVERIFY(observation.accepted);
    QCOMPARE(machine.view().state, MachineState::Applying);

    const CommandResult ackResult = machine.applyCompleted(token, ApplyOutcome::Applied);
    QVERIFY(ackResult.accepted);
    QCOMPARE(ackResult.error, CommandError::None);
    QCOMPARE(machine.view().state, MachineState::Observing);
    QCOMPARE(machine.currentSnapshot(), preimageObserved);
    QVERIFY(port.journalPresent);
    QCOMPARE(port.journal.phase, JournalPhase::Applying);

    const Display::Snapshot targetObserved =
        Test::observed(base, candidate, base.revision + 2);
    QVERIFY(machine.observedSnapshot(targetObserved).accepted);
    QCOMPARE(machine.view().state, MachineState::AwaitingConfirmation);
    QCOMPARE(port.journal.phase, JournalPhase::AwaitingConfirmation);
}

void TransactionStateTests::staleOrWrongLineageObservationDuringApplyIsRejected()
{
    // A stale replay (same revision, different content) or an observation
    // from a foreign service epoch must never be accepted as a retained
    // candidate while Applying; both are rejected without mutating state, and
    // the eventual Applied ack still proceeds normally.
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    QVERIFY(machine.stage(QStringLiteral("tx"), candidate).accepted);
    QVERIFY(machine.preview(QStringLiteral("tx")).accepted);
    const quint64 token = port.requests.last().token;
    const MachineView applying = machine.view();
    const Display::Snapshot beforeAck = machine.currentSnapshot();

    Display::Snapshot staleSameRevision = Test::observed(base, candidate, base.revision);
    QCOMPARE(machine.observedSnapshot(staleSameRevision).error, CommandError::InvalidSnapshot);
    QCOMPARE(machine.view(), applying);
    QCOMPARE(machine.currentSnapshot(), beforeAck);

    Display::Snapshot foreignEpoch = Test::observed(base, candidate, base.revision + 1);
    foreignEpoch.serviceEpoch = QStringLiteral("other-epoch");
    foreignEpoch.liveFingerprint = DisplayTopology::canonicalFingerprint(
        DisplayTopology::candidateFromSnapshot(foreignEpoch));
    QCOMPARE(machine.observedSnapshot(foreignEpoch).error, CommandError::InvalidSnapshot);
    QCOMPARE(machine.view(), applying);
    QCOMPARE(machine.currentSnapshot(), beforeAck);

    QVERIFY(machine.applyCompleted(token, ApplyOutcome::Applied).accepted);
    QCOMPARE(machine.view().state, MachineState::Observing);
    QCOMPARE(machine.currentSnapshot(), beforeAck);
}

void TransactionStateTests::inventoryObservationBeforeRevertAckCompletesNoOpRollback()
{
    // Symmetric to the forward-apply race: the independent inventory update
    // can also announce a completed rollback before the revert's Applied
    // callback arrives. That retained truth must let the existing no-op
    // short-circuit in applyCompleted() fire without waiting for
    // RevertingObserve and a further inventory event.
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToObserving(machine, port, candidate);

    Display::Candidate mismatchCandidate = candidate;
    mismatchCandidate.outputs[0].modeId = QStringLiteral("small");
    const Display::Snapshot mismatch = Test::observed(base, mismatchCandidate, 2);
    QVERIFY(machine.observedSnapshot(mismatch).accepted);
    QCOMPARE(machine.view().state, MachineState::Observing);

    clock.advance(Test::timing().observationTimeoutMilliseconds);
    QCOMPARE(machine.tick().error, CommandError::ObservationTimeout);
    QCOMPARE(machine.view().state, MachineState::RevertingApply);
    const quint64 rollbackToken = port.requests.last().token;

    const Display::Snapshot restored = Test::observed(
        base, DisplayTopology::candidateFromSnapshot(base), 3);
    const CommandResult observation = machine.observedSnapshot(restored);
    QVERIFY(observation.accepted);
    QCOMPARE(machine.view().state, MachineState::RevertingApply);
    QCOMPARE(machine.currentSnapshot(), restored);

    QVERIFY(machine.applyCompleted(rollbackToken, ApplyOutcome::Applied).accepted);
    QCOMPARE(machine.view().state, MachineState::Ready);
    QCOMPARE(machine.currentSnapshot(), restored);
    QCOMPARE(machine.view().lastTerminalReason,
             Display::TransactionReason::ObservationTimeout);
    QVERIFY(!port.journalPresent);
    QCOMPARE(port.requests.size(), 2);
}

QTEST_GUILESS_MAIN(TransactionStateTests)
#include "tst_transaction_state.moc"
