// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/transaction_test_support.h"

#include <QtTest>

using namespace QindaQt::DisplayTransaction;
namespace Display = QindaQt::Display;
namespace DisplayTopology = QindaQt::DisplayTopology;

class TransactionRecoveryTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cancelDeadlineLockAndSuspendRevert();
    void exactlyThreeRevertAttemptsLeadToStuck();
    void silentRevertApplyStillConsumesBoundedAttempts();
    void hotplugWaitsForSettleAndRevertsOnlySurvivingProperties();
    void repeatedTopologyChurnWaitsForLatestExplicitSettle();
    void externalNewerIntentAbortsWithoutFight();
    void crashJournalRecoveryUsesTheSameRevertPath();
};

void TransactionRecoveryTests::cancelDeadlineLockAndSuspendRevert()
{
    enum class Trigger { Cancel, Deadline, Lock, Suspend };
    const QList<Trigger> triggers{Trigger::Cancel, Trigger::Deadline, Trigger::Lock,
                                  Trigger::Suspend};
    for (const Trigger trigger : triggers) {
        Test::FakeClock clock;
        Test::FakePort port;
        Machine machine(clock, port, Test::timing());
        const Display::Snapshot base = Test::snapshot();
        const Display::Candidate candidate = Test::changedCandidate(base);
        QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
        Test::previewToAwaitingConfirmation(machine, port, base, candidate);
        switch (trigger) {
        case Trigger::Cancel:
            QVERIFY(machine.cancel(QStringLiteral("tx")).accepted);
            break;
        case Trigger::Deadline:
            clock.advance(Test::timing().confirmationTimeoutMilliseconds);
            QVERIFY(machine.tick().accepted);
            break;
        case Trigger::Lock:
            QVERIFY(machine.safetyChanged(SafetyState::Locked).accepted);
            break;
        case Trigger::Suspend:
            QVERIFY(machine.prepareForSuspend().accepted);
            break;
        }
        QCOMPARE(machine.view().state, MachineState::RevertingApply);
        const ApplyRequest revert = port.requests.last();
        QCOMPARE(revert.scope, ApplyScope::FullPreimage);
        QVERIFY(machine.applyCompleted(revert.token, ApplyOutcome::Applied).accepted);
        QCOMPARE(machine.view().state, MachineState::RevertingObserve);
        const Display::Snapshot reverted = Test::observed(
            base, DisplayTopology::candidateFromSnapshot(base), 3);
        QVERIFY(machine.observedSnapshot(reverted).accepted);
        QCOMPARE(machine.view().state, MachineState::Ready);
        QVERIFY(!port.journalPresent);
    }
}

void TransactionRecoveryTests::exactlyThreeRevertAttemptsLeadToStuck()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(machine, port, base, candidate);
    QVERIFY(machine.cancel(QStringLiteral("tx")).accepted);

    for (quint32 attempt = 1; attempt <= kMaximumRevertAttempts; ++attempt) {
        QCOMPARE(machine.view().revertAttempt, attempt);
        const quint64 token = port.requests.last().token;
        QVERIFY(machine.applyCompleted(token, ApplyOutcome::Rejected).accepted);
        if (attempt == kMaximumRevertAttempts) {
            QCOMPARE(machine.view().state, MachineState::Stuck);
            break;
        }
        QCOMPARE(machine.view().state, MachineState::RevertBackoff);
        clock.advance(attempt == 1 ? Test::timing().firstRevertBackoffMilliseconds
                                   : Test::timing().secondRevertBackoffMilliseconds);
        QVERIFY(machine.tick().accepted);
        QCOMPARE(machine.view().state, MachineState::RevertingApply);
    }
    QCOMPARE(port.requests.size(), 1 + static_cast<qsizetype>(kMaximumRevertAttempts));
    QVERIFY(port.journalPresent);
    QCOMPARE(port.journal.phase, JournalPhase::Stuck);

    QVERIFY(machine.retryStuck().accepted);
    QCOMPARE(machine.view().state, MachineState::RevertingApply);
    QCOMPARE(machine.view().revertAttempt, quint32(1));
    QVERIFY(port.journalPresent);
}

void TransactionRecoveryTests::silentRevertApplyStillConsumesBoundedAttempts()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(machine, port, base, candidate);
    QVERIFY(machine.cancel(QStringLiteral("tx")).accepted);

    for (quint32 attempt = 1; attempt <= kMaximumRevertAttempts; ++attempt) {
        QCOMPARE(machine.view().state, MachineState::RevertingApply);
        QCOMPARE(machine.view().revertAttempt, attempt);
        clock.advance(Test::timing().applyTimeoutMilliseconds);
        QCOMPARE(machine.tick().error, CommandError::RevertFailed);
        if (attempt == kMaximumRevertAttempts) {
            QCOMPARE(machine.view().state, MachineState::Stuck);
            break;
        }
        QCOMPARE(machine.view().state, MachineState::RevertBackoff);
        clock.advance(attempt == 1 ? Test::timing().firstRevertBackoffMilliseconds
                                   : Test::timing().secondRevertBackoffMilliseconds);
        QVERIFY(machine.tick().accepted);
    }
    QCOMPARE(port.requests.size(), 1 + static_cast<qsizetype>(kMaximumRevertAttempts));
    QCOMPARE(port.journal.phase, JournalPhase::Stuck);
}

void TransactionRecoveryTests::hotplugWaitsForSettleAndRevertsOnlySurvivingProperties()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot(true);
    Display::Candidate candidate = Test::changedCandidate(base);
    candidate.outputs[1].scale = 1.25;
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(machine, port, base, candidate);

    Display::Snapshot changedSet = Test::observed(base, candidate, 3);
    changedSet.outputs.removeFirst();
    changedSet.outputs[0].position = QPoint(0, 0);
    changedSet.outputs[0].priority = 1;
    changedSet.outputs[0].primary = true;
    changedSet.liveFingerprint = DisplayTopology::canonicalFingerprint(
        DisplayTopology::candidateFromSnapshot(changedSet));
    const qsizetype requestsBeforeHotplug = port.requests.size();
    QVERIFY(machine.topologyChanged(changedSet).accepted);
    QCOMPARE(machine.view().state, MachineState::SettlingTopology);
    QCOMPARE(port.requests.size(), requestsBeforeHotplug);

    QVERIFY(machine.topologySettled(changedSet).accepted);
    QCOMPARE(machine.view().state, MachineState::RevertingApply);
    QCOMPARE(port.requests.size(), requestsBeforeHotplug + 1);
    const ApplyRequest revert = port.requests.last();
    QCOMPARE(revert.scope, ApplyScope::SurvivingOutputProperties);
    QVERIFY(revert.candidate.outputs.isEmpty());
    QCOMPARE(revert.survivingProperties.size(), 1);
    QCOMPARE(revert.survivingProperties.first().stableId, QStringLiteral("edid:b"));
    QCOMPARE(revert.survivingProperties.first().scale, 1.0);

    QVERIFY(machine.applyCompleted(revert.token, ApplyOutcome::Applied).accepted);
    Display::Snapshot reverted = changedSet;
    reverted.revision = 4;
    reverted.outputs[0].modeId = revert.survivingProperties.first().modeId;
    reverted.outputs[0].scale = revert.survivingProperties.first().scale;
    reverted.outputs[0].transform = revert.survivingProperties.first().transform;
    reverted.outputs[0].logicalSize = QSize(1920, 1080);
    reverted.liveFingerprint = DisplayTopology::canonicalFingerprint(
        DisplayTopology::candidateFromSnapshot(reverted));
    QVERIFY(machine.observedSnapshot(reverted).accepted);
    QCOMPARE(machine.view().state, MachineState::Ready);
    QCOMPARE(machine.currentSnapshot().outputs[0].position, QPoint(0, 0));
}

void TransactionRecoveryTests::repeatedTopologyChurnWaitsForLatestExplicitSettle()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot(true);
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(machine, port, base, candidate);

    Display::Snapshot first = Test::observed(base, candidate, 3);
    first.outputs.removeFirst();
    first.outputs[0].position = {};
    first.outputs[0].primary = true;
    first.outputs[0].priority = 1;
    first.liveFingerprint = DisplayTopology::canonicalFingerprint(
        DisplayTopology::candidateFromSnapshot(first));
    const qsizetype beforeChurn = port.requests.size();
    QVERIFY(machine.topologyChanged(first).accepted);

    Display::Snapshot latest = first;
    latest.revision = 4;
    latest.outputs[0].scale = 1.25;
    latest.outputs[0].logicalSize = QSize(1536, 864);
    latest.liveFingerprint = DisplayTopology::canonicalFingerprint(
        DisplayTopology::candidateFromSnapshot(latest));
    QVERIFY(machine.topologyChanged(latest).accepted);
    QCOMPARE(machine.view().state, MachineState::SettlingTopology);
    QCOMPARE(port.requests.size(), beforeChurn);

    QVERIFY(machine.topologySettled(latest).accepted);
    QCOMPARE(machine.view().state, MachineState::RevertingApply);
    QCOMPARE(port.requests.size(), beforeChurn + 1);
    QCOMPARE(port.requests.last().scope, ApplyScope::SurvivingOutputProperties);
    QVERIFY(port.requests.last().candidate.outputs.isEmpty());
}

void TransactionRecoveryTests::externalNewerIntentAbortsWithoutFight()
{
    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine(clock, port, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(machine, port, base, candidate);
    const qsizetype requestsBefore = port.requests.size();

    Display::Candidate externalCandidate = candidate;
    externalCandidate.outputs[0].modeId = QStringLiteral("small");
    externalCandidate.outputs[0].scale = 1.0;
    const Display::Snapshot external = Test::observed(base, externalCandidate, 3);
    QCOMPARE(machine.externalIntentObserved(external).error, CommandError::ExternalChange);
    QCOMPARE(machine.view().state, MachineState::Ready);
    QCOMPARE(port.requests.size(), requestsBefore);
    QVERIFY(!port.journalPresent);

    Test::FakeClock failedClearClock;
    Test::FakePort failedClearPort;
    Machine failedClear(failedClearClock, failedClearPort, Test::timing());
    QVERIFY(failedClear.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(failedClear, failedClearPort, base, candidate);
    failedClearPort.clearSucceeds = false;
    QCOMPARE(failedClear.externalIntentObserved(external).error,
             CommandError::JournalFailure);
    QCOMPARE(failedClear.view().state, MachineState::Stuck);
    QCOMPARE(failedClear.view().reason, Display::TransactionReason::JournalFailure);
    QCOMPARE(failedClearPort.journal.reason,
             Display::TransactionReason::ExternalChange);

    Test::FakeClock restartClock;
    Test::FakePort restartPort;
    Machine restart(restartClock, restartPort, Test::timing());
    const Display::Snapshot changedSet = Test::snapshot(true, 4);
    QCOMPARE(restart.recover(failedClearPort.journal, changedSet,
                             SafetyState::Safe).error,
             CommandError::ExternalChange);
    QCOMPARE(restart.view().state, MachineState::SettlingTopology);
    QVERIFY(restartPort.requests.isEmpty());
    QCOMPARE(restart.topologySettled(changedSet).error,
             CommandError::ExternalChange);
    QCOMPARE(restart.view().state, MachineState::Ready);
    QVERIFY(restartPort.requests.isEmpty());
}

void TransactionRecoveryTests::crashJournalRecoveryUsesTheSameRevertPath()
{
    Test::FakeClock firstClock;
    Test::FakePort firstPort;
    Machine first(firstClock, firstPort, Test::timing());
    const Display::Snapshot base = Test::snapshot();
    const Display::Candidate candidate = Test::changedCandidate(base);
    QVERIFY(first.initialize(base, SafetyState::Safe).accepted);
    Test::previewToAwaitingConfirmation(first, firstPort, base, candidate);
    const Journal crashJournal = firstPort.journal;
    QVERIFY(isValidJournal(crashJournal));

    Test::FakeClock recoveryClock;
    Test::FakePort recoveryPort;
    Machine recovery(recoveryClock, recoveryPort, Test::timing());
    Display::Snapshot afterRestart = Test::observed(base, candidate, 9);
    afterRestart.serviceEpoch = QStringLiteral("replacement-service-epoch");
    QVERIFY(recovery.recover(crashJournal, afterRestart, SafetyState::Safe).accepted);
    QCOMPARE(recovery.view().state, MachineState::RevertingApply);
    QCOMPARE(recovery.view().reason, Display::TransactionReason::Recovery);
    QCOMPARE(recoveryPort.requests.size(), 1);
    QCOMPARE(recoveryPort.requests.first().scope, ApplyScope::FullPreimage);
    QCOMPARE(recoveryPort.requests.first().candidate.baseEpoch,
             afterRestart.serviceEpoch);
    QCOMPARE(recoveryPort.requests.first().candidate.baseRevision,
             afterRestart.revision);
}

QTEST_GUILESS_MAIN(TransactionRecoveryTests)
#include "tst_transaction_recovery.moc"
