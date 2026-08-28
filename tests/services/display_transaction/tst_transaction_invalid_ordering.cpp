// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/transaction_test_support.h"

#include <QtTest>

using namespace QindaQt::DisplayTransaction;
namespace Display = QindaQt::Display;
namespace DisplayTopology = QindaQt::DisplayTopology;

namespace
{

class StateFixture
{
public:
    explicit StateFixture(const MachineState requested)
        : machine(clock, port, Test::timing())
        , base(Test::snapshot(true))
        , candidate(Test::changedCandidate(base))
    {
        if (requested == MachineState::Discovering) {
            return;
        }
        Test::require(machine.initialize(base, SafetyState::Safe).accepted,
                      "fixture initialize");
        if (requested == MachineState::Ready) {
            return;
        }
        Test::require(machine.stage(QStringLiteral("tx"), candidate).accepted,
                      "fixture stage");
        if (requested == MachineState::Staged) {
            return;
        }
        Test::require(machine.preview(QStringLiteral("tx")).accepted,
                      "fixture preview");
        if (requested == MachineState::Applying) {
            return;
        }
        const quint64 forwardToken = port.requests.last().token;
        if (requested == MachineState::ResolvingUncertain) {
            Test::require(machine.applyCompleted(forwardToken,
                                                 ApplyOutcome::Rejected).accepted,
                          "fixture uncertain completion");
            return;
        }
        Test::require(machine.applyCompleted(forwardToken, ApplyOutcome::Applied).accepted,
                      "fixture forward completion");
        if (requested == MachineState::Observing) {
            return;
        }
        Test::require(machine.observedSnapshot(
                          Test::observed(base, candidate, 2)).accepted,
                      "fixture target observation");
        if (requested == MachineState::AwaitingConfirmation) {
            return;
        }
        if (requested == MachineState::SettlingTopology) {
            Display::Snapshot one = Test::observed(base, candidate, 3);
            one.outputs.removeFirst();
            one.outputs[0].primary = true;
            one.outputs[0].position = {};
            one.outputs[0].priority = 1;
            one.liveFingerprint = DisplayTopology::canonicalFingerprint(
                DisplayTopology::candidateFromSnapshot(one));
            Test::require(machine.topologyChanged(one).accepted,
                          "fixture topology change");
            return;
        }
        Test::require(machine.cancel(QStringLiteral("tx")).accepted,
                      "fixture cancel");
        if (requested == MachineState::RevertingApply) {
            return;
        }
        if (requested == MachineState::RevertingObserve) {
            Test::require(machine.applyCompleted(port.requests.last().token,
                                                 ApplyOutcome::Applied).accepted,
                          "fixture revert completion");
            return;
        }
        Test::require(requested == MachineState::RevertBackoff
                          || requested == MachineState::Stuck,
                      "fixture requested state");
        Test::require(machine.applyCompleted(port.requests.last().token,
                                             ApplyOutcome::Rejected).accepted,
                      "fixture revert rejection");
        if (requested == MachineState::RevertBackoff) {
            return;
        }
        for (quint32 attempt = 1; attempt < kMaximumRevertAttempts; ++attempt) {
            clock.advance(attempt == 1 ? Test::timing().firstRevertBackoffMilliseconds
                                       : Test::timing().secondRevertBackoffMilliseconds);
            Test::require(machine.tick().accepted, "fixture retry backoff");
            Test::require(machine.applyCompleted(port.requests.last().token,
                                                 ApplyOutcome::Rejected).accepted,
                          "fixture retry rejection");
        }
        Test::require(machine.view().state == MachineState::Stuck,
                      "fixture stuck state");
    }

    Test::FakeClock clock;
    Test::FakePort port;
    Machine machine;
    Display::Snapshot base;
    Display::Candidate candidate;
};

template<typename Command>
void rejectsWithoutMutation(StateFixture &fixture, const CommandError error,
                            Command &&command)
{
    const MachineView view = fixture.machine.view();
    const Display::Snapshot snapshot = fixture.machine.currentSnapshot();
    const Journal journal = fixture.machine.activeJournal();
    const qsizetype requests = fixture.port.requests.size();
    const int stores = fixture.port.storeCalls;
    const int clears = fixture.port.clearCalls;

    const CommandResult result = command();
    QVERIFY(!result.accepted);
    QVERIFY(!result.stateChanged);
    QCOMPARE(result.error, error);
    QCOMPARE(fixture.machine.view(), view);
    QCOMPARE(fixture.machine.currentSnapshot(), snapshot);
    QCOMPARE(fixture.machine.activeJournal(), journal);
    QCOMPARE(fixture.port.requests.size(), requests);
    QCOMPARE(fixture.port.storeCalls, stores);
    QCOMPARE(fixture.port.clearCalls, clears);
}

} // namespace

class TransactionInvalidOrderingTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void discoveringRejectsOutOfOrderInputsExactly();
    void everyActiveStateRejectsWrongTransactionAndRecoveryInputsExactly();
    void stateSpecificCallbacksAndCommandsRejectExactly();
};

void TransactionInvalidOrderingTests::discoveringRejectsOutOfOrderInputsExactly()
{
    StateFixture fixture(MachineState::Discovering);
    rejectsWithoutMutation(fixture, CommandError::InvalidTransition, [&] {
        return fixture.machine.stage(QStringLiteral("tx"), fixture.candidate);
    });
    rejectsWithoutMutation(fixture, CommandError::InvalidTransition, [&] {
        return fixture.machine.preview(QStringLiteral("tx"));
    });
    rejectsWithoutMutation(fixture, CommandError::CallbackOutOfOrder, [&] {
        return fixture.machine.observedSnapshot(fixture.base);
    });
    rejectsWithoutMutation(fixture, CommandError::InvalidTransition, [&] {
        return fixture.machine.externalIntentObserved(fixture.base);
    });
    rejectsWithoutMutation(fixture, CommandError::InvalidTransition, [&] {
        return fixture.machine.topologyChanged(fixture.base);
    });
    rejectsWithoutMutation(fixture, CommandError::InvalidTransition, [&] {
        return fixture.machine.retryStuck();
    });
}

void TransactionInvalidOrderingTests::everyActiveStateRejectsWrongTransactionAndRecoveryInputsExactly()
{
    const QList<MachineState> states{
        MachineState::Staged,          MachineState::Applying,
        MachineState::Observing,       MachineState::AwaitingConfirmation,
        MachineState::ResolvingUncertain, MachineState::SettlingTopology,
        MachineState::RevertingApply,  MachineState::RevertingObserve,
        MachineState::RevertBackoff,   MachineState::Stuck,
    };
    for (const MachineState state : states) {
        StateFixture fixture(state);
        QCOMPARE(fixture.machine.view().state, state);
        rejectsWithoutMutation(fixture, CommandError::TransactionActive, [&] {
            return fixture.machine.stage(QStringLiteral("other"), fixture.candidate);
        });
        rejectsWithoutMutation(fixture, CommandError::UnknownTransaction, [&] {
            return fixture.machine.cancel(QStringLiteral("other"));
        });
        rejectsWithoutMutation(fixture, CommandError::InvalidTransition, [&] {
            return fixture.machine.recover(fixture.machine.activeJournal(), fixture.base,
                                           SafetyState::Safe);
        });
    }
}

void TransactionInvalidOrderingTests::stateSpecificCallbacksAndCommandsRejectExactly()
{
    {
        StateFixture fixture(MachineState::Ready);
        rejectsWithoutMutation(fixture, CommandError::CallbackOutOfOrder, [&] {
            return fixture.machine.applyCompleted(1, ApplyOutcome::Applied);
        });
        rejectsWithoutMutation(fixture, CommandError::InvalidTransition, [&] {
            return fixture.machine.confirm(QStringLiteral("tx"));
        });
        rejectsWithoutMutation(fixture, CommandError::InvalidTransition, [&] {
            return fixture.machine.topologySettled(fixture.base);
        });
    }
    {
        StateFixture fixture(MachineState::Staged);
        rejectsWithoutMutation(fixture, CommandError::UnknownTransaction, [&] {
            return fixture.machine.preview(QStringLiteral("other"));
        });
        rejectsWithoutMutation(fixture, CommandError::CallbackOutOfOrder, [&] {
            return fixture.machine.observedSnapshot(fixture.base);
        });
    }
    for (const MachineState state : {MachineState::Applying,
                                     MachineState::RevertingApply}) {
        StateFixture fixture(state);
        rejectsWithoutMutation(fixture, CommandError::CallbackOutOfOrder, [&] {
            return fixture.machine.applyCompleted(999, ApplyOutcome::Applied);
        });
    }
    const QList<MachineState> tokenlessStates{
        MachineState::Observing, MachineState::AwaitingConfirmation,
        MachineState::ResolvingUncertain, MachineState::SettlingTopology,
        MachineState::RevertingObserve, MachineState::RevertBackoff,
        MachineState::Stuck,
    };
    for (const MachineState state : tokenlessStates) {
        StateFixture fixture(state);
        rejectsWithoutMutation(fixture, CommandError::CallbackOutOfOrder, [&] {
            return fixture.machine.applyCompleted(999, ApplyOutcome::Applied);
        });
    }
    {
        StateFixture fixture(MachineState::Stuck);
        rejectsWithoutMutation(fixture, CommandError::InvalidTransition, [&] {
            return fixture.machine.confirm(QStringLiteral("tx"));
        });
        rejectsWithoutMutation(fixture, CommandError::InvalidTransition, [&] {
            return fixture.machine.topologySettled(fixture.base);
        });
    }
}

QTEST_GUILESS_MAIN(TransactionInvalidOrderingTests)
#include "tst_transaction_invalid_ordering.moc"
