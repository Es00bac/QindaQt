// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_types.h>
#include <qindaqt/services/display_protocol/display_validation.h>
#include <qindaqt/services/display_transaction/transaction_types.h>

#include <optional>

namespace QindaQt::DisplayService::Private
{

// Maps the resident machine state onto the closed public TransactionState
// enum. Discovering and Ready carry no active transaction and therefore map
// to no summary. AGENT-NOTE: the protocol's PersistingJournal value is never
// published by this resident because journal storage is a synchronous hard
// gate inside preview, not an observable machine state; do not invent an
// intermediate publication without a protocol decision.
[[nodiscard]] inline std::optional<Display::TransactionState> publicTransactionState(
    const DisplayTransaction::MachineState state)
{
    using MachineState = DisplayTransaction::MachineState;
    switch (state) {
    case MachineState::Staged:
        return Display::TransactionState::Staged;
    case MachineState::Applying:
        return Display::TransactionState::Applying;
    case MachineState::Observing:
        return Display::TransactionState::Observing;
    case MachineState::AwaitingConfirmation:
        return Display::TransactionState::AwaitingConfirmation;
    case MachineState::SettlingTopology:
        return Display::TransactionState::SettlingTopology;
    case MachineState::ResolvingUncertain:
        return Display::TransactionState::ResolvingUncertain;
    case MachineState::RevertingApply:
    case MachineState::RevertingObserve:
    case MachineState::RevertBackoff:
        return Display::TransactionState::Reverting;
    case MachineState::Stuck:
        return Display::TransactionState::Stuck;
    case MachineState::Discovering:
    case MachineState::Ready:
        break;
    }
    return std::nullopt;
}

// AGENT-CONTRACT: the resident publishes the machine view's active
// transaction as exactly zero or one validated public TransactionSummary.
// `baseRevision` is the revision the staged candidate was built from (the
// journal target lineage); `observedRevision` is the machine's current
// accepted revision; `deadlineMonotonicMilliseconds` is the injected
// service-clock deadline, zero while none is armed. AGENT-GUARD: any
// incomplete, unbounded, or lineage-inconsistent projection must fail
// closed to no summary; publishing a partial or unvalidated public value
// here would let consumers trust a transaction the resident cannot prove.
[[nodiscard]] inline std::optional<Display::TransactionSummary>
projectedTransactionSummary(const Display::Snapshot &snapshot,
                            const DisplayTransaction::MachineView &view,
                            const DisplayTransaction::Journal &journal)
{
    const std::optional<Display::TransactionState> state =
        publicTransactionState(view.state);
    if (!state.has_value() || view.transactionId.isEmpty()) {
        return std::nullopt;
    }
    const Display::TransactionSummary summary{
        .transactionId = view.transactionId,
        .state = *state,
        .reason = view.reason,
        .initiatingEpoch = snapshot.serviceEpoch,
        .baseRevision = journal.target.baseRevision,
        .observedRevision = view.currentRevision,
        .deadlineMonotonicMilliseconds = view.deadlineMonotonicMilliseconds,
        .revertAttempt = view.revertAttempt};
    if (!Display::validateTransactionSummary(summary).accepted
        || summary.initiatingEpoch != snapshot.serviceEpoch
        || summary.observedRevision > snapshot.revision) {
        return std::nullopt;
    }
    return summary;
}

} // namespace QindaQt::DisplayService::Private
