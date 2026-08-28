// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/transaction_test_support.h"

#include <QtTest>

using namespace QindaQt::DisplayTransaction;
namespace Display = QindaQt::Display;
namespace DisplayTopology = QindaQt::DisplayTopology;

class TransactionStateTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stageFencesRevisionAndDetectsNoOp();
    void previewRequiresSafeAuthorityAndDurableJournal();
    void applyObserveConfirmFlow();
    void readyInputsEnforceCurrentLineage();
    void rejectedAndTimedOutApplyNeverReplayForward();
    void observationMismatchTimeoutAndInvalidCallbacks();
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
    for (const bool topologyInput : {false, true}) {
        Test::FakeClock clock;
        Test::FakePort port;
        Machine machine(clock, port, Test::timing());
        QVERIFY(machine.initialize(base, SafetyState::Safe).accepted);
        const MachineView beforeView = machine.view();
        const Display::Snapshot beforeSnapshot = machine.currentSnapshot();

        Display::Snapshot older = base;
        older.revision--;
        const CommandResult olderResult = topologyInput
            ? machine.topologyChanged(older)
            : machine.externalIntentObserved(older);
        QCOMPARE(olderResult.error, CommandError::InvalidSnapshot);
        QCOMPARE(machine.view(), beforeView);
        QCOMPARE(machine.currentSnapshot(), beforeSnapshot);

        Display::Snapshot otherEpoch = base;
        otherEpoch.serviceEpoch = QStringLiteral("other-epoch");
        otherEpoch.revision++;
        const CommandResult otherEpochResult = topologyInput
            ? machine.topologyChanged(otherEpoch)
            : machine.externalIntentObserved(otherEpoch);
        QCOMPARE(otherEpochResult.error, CommandError::InvalidSnapshot);
        QCOMPARE(machine.view(), beforeView);
        QCOMPARE(machine.currentSnapshot(), beforeSnapshot);

        Display::Snapshot newer = base;
        newer.revision++;
        const CommandResult newerResult = topologyInput
            ? machine.topologyChanged(newer)
            : machine.externalIntentObserved(newer);
        QVERIFY(newerResult.accepted);
        QCOMPARE(machine.currentSnapshot().revision, newer.revision);
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
    QVERIFY(port.requests.isEmpty());

    port.storeSucceeds = true;
    QVERIFY(machine.preview(QStringLiteral("tx")).accepted);
    QCOMPARE(machine.view().state, MachineState::Applying);
    QVERIFY(machine.view().journalActive);
    QCOMPARE(port.requests.size(), 1);
    QCOMPARE(port.requests.first().scope, ApplyScope::ForwardCandidate);
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

QTEST_GUILESS_MAIN(TransactionStateTests)
#include "tst_transaction_state.moc"
