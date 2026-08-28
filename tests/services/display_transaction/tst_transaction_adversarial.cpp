// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/transaction_test_support.h"

#include <QtTest>

using namespace QindaQt::DisplayTransaction;
namespace Display = QindaQt::Display;
namespace DisplayTopology = QindaQt::DisplayTopology;

namespace
{

Display::Snapshot removeFirstOutput(Display::Snapshot snapshot, const quint64 revision)
{
    snapshot.revision = revision;
    snapshot.outputs.removeFirst();
    snapshot.outputs[0].enabled = true;
    snapshot.outputs[0].primary = true;
    snapshot.outputs[0].position = {};
    snapshot.outputs[0].priority = 1;
    snapshot.outputs[0].replicationSourceStableId.clear();
    snapshot.liveFingerprint = DisplayTopology::canonicalFingerprint(
        DisplayTopology::candidateFromSnapshot(snapshot));
    return snapshot;
}

Journal activeJournal(const Display::Snapshot &base, const Display::Candidate &candidate)
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    Test::require(machine.initialize(base, SafetyState::Safe).accepted, "journal initialize");
    Test::previewToAwaitingConfirmation(machine, port, base, candidate);
    Test::require(port.journalPresent, "active journal");
    return port.journal;
}

void failCurrentRevert(Machine &machine, Test::FakeClock &clock,
                       Test::FakePort &port)
{
    const quint64 token = port.requests.last().token;
    Test::require(machine.applyCompleted(token, ApplyOutcome::Rejected).accepted,
                  "revert rejection");
    if (machine.view().state == MachineState::RevertBackoff) {
        const quint64 backoff = machine.view().revertAttempt == 1
            ? Test::timing().firstRevertBackoffMilliseconds
            : Test::timing().secondRevertBackoffMilliseconds;
        clock.advance(backoff);
        Test::require(machine.tick().accepted, "revert backoff");
    }
}

} // namespace

class TransactionAdversarialTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectedCommandsAndJournalGatesPreserveState();
    void rollbackBudgetCannotBeRestartedByRepeatedSafetyInputs();
    void settleBarrierDefersEveryActionAndRestoredSetUsesFullPreimage();
    void cleanupOnlyStuckNeverIssuesAnApply();
    void uncertaintyAndObservationRoutingAreExplicit();
    void recoveryDoesNotFightExternalTruthAndStuckAdoptsTopology();
    void disabledPreimageAndAlreadyRestoredSurvivorsDoNotApply();
    void terminalReasonAndStateChangedRemainObservable();
};

void TransactionAdversarialTests::rejectedCommandsAndJournalGatesPreserveState()
{
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);

    const MachineView ready = machine.view();
    const Display::Snapshot readySnapshot = machine.currentSnapshot();
    const Journal readyJournal = machine.activeJournal();
    QCOMPARE(machine.stage({}, candidate).error, CommandError::InvalidTransactionId);
    QCOMPARE(machine.preview(QStringLiteral("tx")).error,
             CommandError::InvalidTransition);
    QCOMPARE(machine.confirm(QStringLiteral("tx")).error,
             CommandError::InvalidTransition);
    QCOMPARE(machine.applyCompleted(1, ApplyOutcome::Applied).error,
             CommandError::CallbackOutOfOrder);
    QCOMPARE(machine.retryStuck().error, CommandError::InvalidTransition);
    QCOMPARE(machine.view(), ready);
    QCOMPARE(machine.currentSnapshot(), readySnapshot);
    QCOMPARE(machine.activeJournal(), readyJournal);
    QVERIFY(port.requests.isEmpty());

    QVERIFY(machine.stage(QStringLiteral("tx"), candidate).accepted);
    const MachineView staged = machine.view();
    const Journal stagedJournal = machine.activeJournal();
    port.storeSucceeds = false;
    QCOMPARE(machine.preview(QStringLiteral("tx")).error, CommandError::JournalFailure);
    QCOMPARE(machine.view(), staged);
    QCOMPARE(machine.activeJournal(), stagedJournal);
    QVERIFY(port.requests.isEmpty());

    Test::FakePort suspendPort;
    Machine suspendMachine(clock, suspendPort, Test::timing());
    QVERIFY(suspendMachine.initialize(base, SafetyState::Safe).accepted);
    QVERIFY(suspendMachine.stage(QStringLiteral("suspend"), candidate).accepted);
    QCOMPARE(suspendMachine.prepareForSuspend().error, CommandError::Suspend);
    QCOMPARE(suspendMachine.view().state, MachineState::Ready);
    QCOMPARE(suspendMachine.view().lastTerminalReason,
             Display::TransactionReason::Suspend);

    Test::FakePort confirmPort;
    Machine confirmMachine(clock, confirmPort, Test::timing());
    QVERIFY(confirmMachine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(confirmMachine, confirmPort, base, candidate);
    const MachineView awaiting = confirmMachine.view();
    const Journal awaitingJournal = confirmMachine.activeJournal();
    const Display::Snapshot awaitingSnapshot = confirmMachine.currentSnapshot();
    const qsizetype requests = confirmPort.requests.size();
    confirmPort.clearSucceeds = false;
    const CommandResult failedConfirm = confirmMachine.confirm(QStringLiteral("tx"));
    QVERIFY(!failedConfirm.accepted);
    QCOMPARE(failedConfirm.error, CommandError::JournalFailure);
    QCOMPARE(confirmMachine.view(), awaiting);
    QCOMPARE(confirmMachine.activeJournal(), awaitingJournal);
    QCOMPARE(confirmMachine.currentSnapshot(), awaitingSnapshot);
    QCOMPARE(confirmPort.requests.size(), requests);
}

void TransactionAdversarialTests::rollbackBudgetCannotBeRestartedByRepeatedSafetyInputs()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(machine, port, base, candidate);
    QVERIFY(machine.cancel(QStringLiteral("tx")).accepted);

    failCurrentRevert(machine, clock, port);
    QCOMPARE(machine.view().revertAttempt, quint32(2));
    const qsizetype afterSecondIssue = port.requests.size();
    QVERIFY(!machine.cancel(QStringLiteral("tx")).stateChanged);
    QVERIFY(machine.safetyChanged(SafetyState::Locked).accepted);
    QVERIFY(machine.safetyChanged(SafetyState::Safe).accepted);
    QVERIFY(machine.safetyChanged(SafetyState::Locked).accepted);
    QCOMPARE(machine.prepareForSuspend().error, CommandError::Suspend);
    QCOMPARE(machine.view().state, MachineState::RevertingApply);
    QCOMPARE(machine.view().revertAttempt, quint32(2));
    QCOMPARE(port.requests.size(), afterSecondIssue);

    failCurrentRevert(machine, clock, port);
    QCOMPARE(machine.view().state, MachineState::RevertingApply);
    QCOMPARE(machine.view().revertAttempt, quint32(3));
    const quint64 lastToken = port.requests.last().token;
    QVERIFY(machine.applyCompleted(lastToken, ApplyOutcome::Rejected).accepted);
    QCOMPARE(machine.view().state, MachineState::Stuck);
    QCOMPARE(port.requests.size(), 1 + static_cast<qsizetype>(kMaximumRevertAttempts));
}

void TransactionAdversarialTests::settleBarrierDefersEveryActionAndRestoredSetUsesFullPreimage()
{
    const Display::Snapshot base = Test::snapshot(true);
    Display::Candidate candidate = Test::changedCandidate(base);
    candidate.outputs[1].scale = 1.25;

    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(machine, port, base, candidate);
    Display::Snapshot changedSet = removeFirstOutput(
        Test::observed(base, candidate, 3), 3);
    QVERIFY(machine.topologyChanged(changedSet).accepted);
    const qsizetype requestsBefore = port.requests.size();
    const int clearsBefore = port.clearCalls;
    QVERIFY(machine.cancel(QStringLiteral("tx")).accepted);
    QCOMPARE(machine.safetyChanged(SafetyState::Locked).error, CommandError::Locked);
    QCOMPARE(machine.prepareForSuspend().error, CommandError::Suspend);
    changedSet.revision++;
    QVERIFY(machine.externalIntentObserved(changedSet).accepted);
    QCOMPARE(machine.view().state, MachineState::SettlingTopology);
    QCOMPARE(port.requests.size(), requestsBefore);
    QCOMPARE(port.clearCalls, clearsBefore);
    const Journal deferredExternal = port.journal;
    QVERIFY(isValidJournal(deferredExternal));
    QCOMPARE(machine.topologySettled(changedSet).error, CommandError::ExternalChange);
    QCOMPARE(machine.view().state, MachineState::Ready);
    QCOMPARE(port.requests.size(), requestsBefore);

    Test::FakeClock externalRecoveryClock;
    Test::FakePort externalRecoveryPort;
    Machine externalRecovery(externalRecoveryClock, externalRecoveryPort,
                             Test::timing());
    QCOMPARE(externalRecovery.recover(deferredExternal, changedSet,
                                       SafetyState::Safe).error,
             CommandError::ExternalChange);
    QCOMPARE(externalRecovery.view().state, MachineState::SettlingTopology);
    QVERIFY(externalRecoveryPort.requests.isEmpty());
    QCOMPARE(externalRecovery.topologySettled(changedSet).error,
             CommandError::ExternalChange);
    QCOMPARE(externalRecovery.view().state, MachineState::Ready);
    QVERIFY(externalRecoveryPort.requests.isEmpty());

    Test::FakeClock flapClock;
    Test::FakePort flapPort;
    Machine flap(flapClock, flapPort, Test::timing());
    QVERIFY(flap.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(flap, flapPort, base, candidate);
    QVERIFY(flap.topologyChanged(removeFirstOutput(
        Test::observed(base, candidate, 3), 3)).accepted);
    const qsizetype flapRequests = flapPort.requests.size();
    const Display::Snapshot originalSet = Test::observed(base, candidate, 4);
    QVERIFY(flap.topologySettled(originalSet).accepted);
    QCOMPARE(flap.view().state, MachineState::RevertingApply);
    QCOMPARE(flapPort.requests.size(), flapRequests + 1);
    QCOMPARE(flapPort.requests.last().scope, ApplyScope::FullPreimage);
}

void TransactionAdversarialTests::cleanupOnlyStuckNeverIssuesAnApply()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(machine, port, base, candidate);
    QVERIFY(machine.cancel(QStringLiteral("tx")).accepted);
    const ApplyRequest revert = port.requests.last();
    QVERIFY(machine.applyCompleted(revert.token, ApplyOutcome::Applied).accepted);
    port.clearSucceeds = false;
    const Display::Snapshot reverted = Test::observed(
        base, DisplayTopology::candidateFromSnapshot(base), 3);
    QCOMPARE(machine.observedSnapshot(reverted).error, CommandError::JournalFailure);
    QCOMPARE(machine.view().state, MachineState::Stuck);
    QCOMPARE(machine.view().reason, Display::TransactionReason::JournalFailure);
    QCOMPARE(machine.view().revertAttempt, quint32(0));
    const qsizetype requestsBeforeRetry = port.requests.size();
    port.clearSucceeds = true;
    QVERIFY(machine.retryStuck().accepted);
    QCOMPARE(machine.view().state, MachineState::Ready);
    QCOMPARE(port.requests.size(), requestsBeforeRetry);
}

void TransactionAdversarialTests::uncertaintyAndObservationRoutingAreExplicit()
{
    Display::Snapshot translated = Test::snapshot();
    translated.outputs[0].position = QPoint(100, 50);
    translated.liveFingerprint = DisplayTopology::canonicalFingerprint(
        DisplayTopology::candidateFromSnapshot(translated));
    Test::FakeClock translatedClock;
    Test::FakePort translatedPort;
    Machine translatedMachine(translatedClock, translatedPort, Test::timing());
    QVERIFY(translatedMachine.initialize(translated, SafetyState::Safe).accepted);

    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    QVERIFY(machine.stage(QStringLiteral("tx"), candidate).accepted);
    QVERIFY(machine.preview(QStringLiteral("tx")).accepted);
    QCOMPARE(machine.applyCompleted(port.requests.last().token,
                                    ApplyOutcome::TransportUncertain).error,
             CommandError::ApplyUncertain);
    QCOMPARE(machine.view().reason, Display::TransactionReason::TransportUncertain);
    Display::Candidate mismatchCandidate = candidate;
    mismatchCandidate.outputs[0].modeId = QStringLiteral("small");
    const Display::Snapshot mismatch = Test::observed(base, mismatchCandidate, 2);
    QVERIFY(machine.observedSnapshot(mismatch).stateChanged);
    QVERIFY(!machine.observedSnapshot(mismatch).stateChanged);
    clock.advance(Test::timing().observationTimeoutMilliseconds);
    QVERIFY(machine.tick().accepted);
    QCOMPARE(port.requests.last().scope, ApplyScope::FullPreimage);

    Test::FakeClock rejectClock;
    Test::FakePort rejectPort;
    Machine rejected(rejectClock, rejectPort, Test::timing());
    QVERIFY(rejected.initialize(base, SafetyState::Safe).accepted);
    QVERIFY(rejected.stage(QStringLiteral("reject"), candidate).accepted);
    QVERIFY(rejected.preview(QStringLiteral("reject")).accepted);
    QVERIFY(rejected.applyCompleted(rejectPort.requests.last().token,
                                    ApplyOutcome::Rejected).accepted);
    rejectClock.advance(Test::timing().observationTimeoutMilliseconds);
    QVERIFY(rejected.tick().accepted);
    QCOMPARE(rejectPort.requests.size(), 2);
    QCOMPARE(rejectPort.requests.last().scope, ApplyScope::FullPreimage);

    Test::FakeClock pendingClock;
    Test::FakePort pendingPort;
    Machine pending(pendingClock, pendingPort, Test::timing());
    QVERIFY(pending.initialize(base, SafetyState::Safe).accepted);
    QVERIFY(pending.stage(QStringLiteral("pending"), candidate).accepted);
    QVERIFY(pending.preview(QStringLiteral("pending")).accepted);
    const quint64 pendingToken = pendingPort.requests.last().token;
    QVERIFY(pending.cancel(QStringLiteral("pending")).accepted);
    QCOMPARE(pending.view().state, MachineState::Applying);
    QCOMPARE(pendingPort.requests.size(), 1);
    QCOMPARE(pendingPort.journal.reason, Display::TransactionReason::Cancelled);
    QVERIFY(pending.applyCompleted(pendingToken, ApplyOutcome::Applied).accepted);
    QCOMPARE(pending.view().state, MachineState::ResolvingUncertain);
    QVERIFY(pending.observedSnapshot(Test::observed(base, candidate, 2)).accepted);
    QCOMPARE(pending.view().state, MachineState::RevertingApply);
    QCOMPARE(pendingPort.requests.size(), 2);
    QCOMPARE(pending.view().reason, Display::TransactionReason::Cancelled);

    const Display::Snapshot dual = Test::snapshot(true);
    const Display::Candidate dualCandidate = Test::changedCandidate(dual);
    Test::FakeClock routeClock;
    Test::FakePort routePort;
    Machine routed(routeClock, routePort, Test::timing());
    QVERIFY(routed.initialize(dual, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(routed, routePort, dual, dualCandidate);
    const qsizetype beforeSetChange = routePort.requests.size();
    const Display::Snapshot one = removeFirstOutput(
        Test::observed(dual, dualCandidate, 3), 3);
    QVERIFY(routed.observedSnapshot(one).accepted);
    QCOMPARE(routed.view().state, MachineState::SettlingTopology);
    QVERIFY(routePort.journalPresent);
    QCOMPARE(routePort.requests.size(), beforeSetChange);
}

void TransactionAdversarialTests::recoveryDoesNotFightExternalTruthAndStuckAdoptsTopology()
{
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    const Journal journal = activeJournal(base, candidate);

    Display::Candidate externalCandidate = candidate;
    externalCandidate.outputs[0].modeId = QStringLiteral("small");
    externalCandidate.outputs[0].scale = 1.0;
    const Display::Snapshot external = Test::observed(base, externalCandidate, 7);
    Test::FakeClock externalClock;
    Test::FakePort externalPort;
    Machine externalRecovery(externalClock, externalPort, Test::timing());
    QCOMPARE(externalRecovery.recover(journal, external, SafetyState::Safe).error,
             CommandError::ExternalChange);
    QCOMPARE(externalRecovery.view().state, MachineState::Ready);
    QCOMPARE(externalPort.requests.size(), 0);
    QCOMPARE(externalRecovery.view().lastTerminalReason,
             Display::TransactionReason::ExternalChange);

    Test::FakeClock preimageClock;
    Test::FakePort preimagePort;
    Machine preimageRecovery(preimageClock, preimagePort, Test::timing());
    QVERIFY(preimageRecovery.recover(journal, base, SafetyState::Safe).accepted);
    QCOMPARE(preimageRecovery.view().state, MachineState::Ready);
    QVERIFY(preimagePort.requests.isEmpty());

    Test::FakeClock targetClock;
    Test::FakePort targetPort;
    Machine targetRecovery(targetClock, targetPort, Test::timing());
    QVERIFY(targetRecovery.recover(journal, Test::observed(base, candidate, 8),
                                   SafetyState::Safe).accepted);
    QCOMPARE(targetPort.requests.last().scope, ApplyScope::FullPreimage);

    Test::FakeClock changedSetClock;
    Test::FakePort changedSetPort;
    Machine changedSetRecovery(changedSetClock, changedSetPort, Test::timing());
    const Display::Snapshot changedSet = Test::snapshot(true, 9);
    QVERIFY(changedSetRecovery.recover(journal, changedSet,
                                       SafetyState::Safe).accepted);
    QCOMPARE(changedSetRecovery.view().state, MachineState::SettlingTopology);
    QVERIFY(changedSetPort.requests.isEmpty());

    Test::FakeClock stuckClock;
    Test::FakePort stuckPort;
    Machine stuck(stuckClock, stuckPort, Test::timing());
    QVERIFY(stuck.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(stuck, stuckPort, base, candidate);
    QVERIFY(stuck.cancel(QStringLiteral("tx")).accepted);
    failCurrentRevert(stuck, stuckClock, stuckPort);
    failCurrentRevert(stuck, stuckClock, stuckPort);
    QVERIFY(stuck.applyCompleted(stuckPort.requests.last().token,
                                 ApplyOutcome::Rejected).accepted);
    QCOMPARE(stuck.view().state, MachineState::Stuck);
    const Display::Snapshot dual = Test::snapshot(true, 10);
    QVERIFY(stuck.topologyChanged(dual).accepted);
    const qsizetype beforeRetry = stuckPort.requests.size();
    QVERIFY(stuck.retryStuck().accepted);
    QCOMPARE(stuckPort.requests.size(), beforeRetry + 1);
    QCOMPARE(stuckPort.requests.last().scope, ApplyScope::SurvivingOutputProperties);
    QCOMPARE(stuckPort.requests.last().survivingProperties.size(), 1);
    QCOMPARE(stuckPort.requests.last().survivingProperties.first().stableId,
             QStringLiteral("edid:a"));
}

void TransactionAdversarialTests::disabledPreimageAndAlreadyRestoredSurvivorsDoNotApply()
{
    Display::Snapshot disabledBase = Test::snapshot(true);
    Display::Output &disabled = disabledBase.outputs[1];
    disabled.enabled = false;
    disabled.primary = false;
    disabled.modeId.clear();
    disabled.position = {};
    disabled.logicalSize = QSize(0, 0);
    disabled.priority = 0;
    disabledBase.liveFingerprint = DisplayTopology::canonicalFingerprint(
        DisplayTopology::candidateFromSnapshot(disabledBase));
    Display::Candidate enables = DisplayTopology::candidateFromSnapshot(disabledBase);
    Display::CandidateOutput &enabled = enables.outputs[1];
    enabled.enabled = true;
    enabled.modeId = QStringLiteral("full");
    enabled.position = QPoint(1920, 0);
    enabled.priority = 2;

    Test::FakeClock disabledClock;
    Test::FakePort disabledPort;
    Machine disabledMachine(disabledClock, disabledPort, Test::timing());
    QVERIFY(disabledMachine.initialize(disabledBase, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(disabledMachine, disabledPort,
                                        disabledBase, enables);
    const Display::Snapshot onlyFormerlyDisabled = removeFirstOutput(
        Test::observed(disabledBase, enables, 3), 3);
    const qsizetype requests = disabledPort.requests.size();
    QVERIFY(disabledMachine.topologyChanged(onlyFormerlyDisabled).accepted);
    QVERIFY(disabledMachine.topologySettled(onlyFormerlyDisabled).accepted);
    QCOMPARE(disabledMachine.view().state, MachineState::Ready);
    QCOMPARE(disabledPort.requests.size(), requests);

    const Display::Snapshot base = Test::snapshot(true);
    const Display::Candidate candidate = Test::changedCandidate(base);
    Test::FakeClock restoredClock;
    Test::FakePort restoredPort;
    Machine restored(restoredClock, restoredPort, Test::timing());
    QVERIFY(restored.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(restored, restoredPort, base, candidate);
    const Display::Snapshot survivorAlreadyRestored = removeFirstOutput(
        Test::observed(base, candidate, 3), 3);
    const qsizetype beforeSettle = restoredPort.requests.size();
    QVERIFY(restored.topologyChanged(survivorAlreadyRestored).accepted);
    QVERIFY(restored.topologySettled(survivorAlreadyRestored).accepted);
    QCOMPARE(restored.view().state, MachineState::Ready);
    QCOMPARE(restoredPort.requests.size(), beforeSettle);
}

void TransactionAdversarialTests::terminalReasonAndStateChangedRemainObservable()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    QVERIFY(!machine.topologyChanged(base).stateChanged);
    Test::previewToAwaitingConfirmation(machine, port, base, candidate);
    QVERIFY(machine.cancel(QStringLiteral("tx")).accepted);
    const ApplyRequest revert = port.requests.last();
    QVERIFY(machine.applyCompleted(revert.token, ApplyOutcome::Applied).accepted);
    QVERIFY(machine.observedSnapshot(Test::observed(
        base, DisplayTopology::candidateFromSnapshot(base), 3)).accepted);
    QCOMPARE(machine.view().state, MachineState::Ready);
    QCOMPARE(machine.view().reason, Display::TransactionReason::None);
    QCOMPARE(machine.view().lastTerminalReason, Display::TransactionReason::Cancelled);
    QVERIFY(machine.stage(QStringLiteral("next"),
                          Test::changedCandidate(machine.currentSnapshot())).accepted);
    QCOMPARE(machine.view().lastTerminalReason, Display::TransactionReason::None);
}

QTEST_GUILESS_MAIN(TransactionAdversarialTests)
#include "tst_transaction_adversarial.moc"
