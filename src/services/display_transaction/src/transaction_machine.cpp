// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_transaction/transaction_machine.h>

#include <qindaqt/services/display_protocol/display_validation.h>
#include <qindaqt/services/display_topology/topology.h>

#include "transaction_machine_p.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace QindaQt::DisplayTransaction
{

Machine::Machine(MonotonicClock &clock, SideEffectPort &port, Timing timing)
    : m_clock(clock)
    , m_port(port)
    , m_timing(timing)
{
    m_timing.applyTimeoutMilliseconds = std::max<quint64>(1, m_timing.applyTimeoutMilliseconds);
    m_timing.observationTimeoutMilliseconds =
        std::max<quint64>(1, m_timing.observationTimeoutMilliseconds);
    m_timing.confirmationTimeoutMilliseconds =
        std::max<quint64>(1, m_timing.confirmationTimeoutMilliseconds);
    m_timing.firstRevertBackoffMilliseconds =
        std::max<quint64>(1, m_timing.firstRevertBackoffMilliseconds);
    m_timing.secondRevertBackoffMilliseconds =
        std::max<quint64>(1, m_timing.secondRevertBackoffMilliseconds);
}

const MachineView &Machine::view() const noexcept
{
    return m_view;
}

const Display::Snapshot &Machine::currentSnapshot() const noexcept
{
    return m_snapshot;
}

const Journal &Machine::activeJournal() const noexcept
{
    return m_journal;
}

CommandResult Machine::rejected(const CommandError error) const
{
    return {.accepted = false,
            .stateChanged = false,
            .error = error,
            .state = m_view.state,
            .transactionId = m_view.transactionId};
}

CommandResult Machine::accepted(const bool changed, const CommandError error) const
{
    return {.accepted = true,
            .stateChanged = changed,
            .error = error,
            .state = m_view.state,
            .transactionId = m_view.transactionId};
}

bool Machine::validSnapshot(const Display::Snapshot &snapshot) const
{
    // AGENT-GUARD: Live compositor truth need not itself be a valid mutation
    // candidate (for example it may have a translated origin). Snapshot
    // acceptance therefore validates the published value and its canonical
    // projection only; strict topology policy belongs to stage().
    if (!Display::validateSnapshot(snapshot).accepted) {
        return false;
    }
    const Display::Candidate candidate = DisplayTopology::candidateFromSnapshot(snapshot);
    return DisplayTopology::canonicalFingerprint(candidate) == snapshot.liveFingerprint;
}

bool Machine::transactionMatches(const QString &transactionId) const
{
    return !m_view.transactionId.isEmpty() && transactionId == m_view.transactionId;
}

quint64 Machine::nextToken()
{
    const quint64 token = m_nextToken;
    if (m_nextToken == std::numeric_limits<quint64>::max()) {
        m_nextToken = 1;
    } else {
        ++m_nextToken;
    }
    return token;
}

void Machine::setState(const MachineState state)
{
    m_view.state = state;
}

void Machine::clearTransaction()
{
    if (m_view.reason != Display::TransactionReason::None) {
        m_view.lastTerminalReason = m_view.reason;
    }
    m_staged = {};
    m_preimage = {};
    m_journal = {};
    m_survivingProperties.clear();
    m_activeToken = 0;
    m_revertRequested = false;
    m_abandonAfterSettle = false;
    m_cleanupOnlyStuck = false;
    m_view.transactionId.clear();
    m_view.reason = Display::TransactionReason::None;
    m_view.deadlineMonotonicMilliseconds = 0;
    m_view.revertAttempt = 0;
    m_view.journalActive = false;
    setState(MachineState::Ready);
}

void Machine::finishReady(const Display::Snapshot &snapshot)
{
    m_snapshot = snapshot;
    m_view.currentRevision = snapshot.revision;
    clearTransaction();
}

CommandResult Machine::initialize(const Display::Snapshot &snapshot, const SafetyState safety)
{
    if (m_view.state != MachineState::Discovering) {
        return rejected(CommandError::InvalidTransition);
    }
    if (!validSnapshot(snapshot)) {
        return rejected(CommandError::InvalidSnapshot);
    }
    m_snapshot = snapshot;
    m_view.safety = safety;
    m_view.currentRevision = snapshot.revision;
    setState(MachineState::Ready);
    return accepted(true);
}

CommandResult Machine::stage(const QString &transactionId,
                             const Display::Candidate &candidate)
{
    if (m_view.state != MachineState::Ready) {
        return rejected(Private::activeState(m_view.state) ? CommandError::TransactionActive
                                                          : CommandError::InvalidTransition);
    }
    if (!Private::validTransactionId(transactionId)) {
        return rejected(CommandError::InvalidTransactionId);
    }
    if (candidate.baseEpoch != m_snapshot.serviceEpoch
        || candidate.baseRevision != m_snapshot.revision) {
        return rejected(CommandError::StaleRevision);
    }
    if (!Display::validateCandidate(candidate).accepted) {
        return rejected(CommandError::InvalidCandidate);
    }
    const DisplayTopology::ValidationResult validation =
        DisplayTopology::validateAndNormalize(m_snapshot, candidate);
    if (!validation.accepted()) {
        return rejected(CommandError::InvalidCandidate);
    }
    if (validation.noOp) {
        return accepted(false, CommandError::NoOp);
    }

    m_staged = validation.normalizedCandidate;
    m_preimage = DisplayTopology::candidateFromSnapshot(m_snapshot);
    m_journal = {.schemaVersion = kJournalSchemaVersion,
                 .transactionId = transactionId,
                 .phase = JournalPhase::Applying,
                 .reason = Display::TransactionReason::None,
                 .preimage = m_preimage,
                 .target = m_staged,
                 .revertAttempt = 0};
    m_view.transactionId = transactionId;
    m_view.reason = Display::TransactionReason::None;
    m_view.lastTerminalReason = Display::TransactionReason::None;
    setState(MachineState::Staged);
    return accepted(true);
}

void Machine::beginForwardApply()
{
    setState(MachineState::Applying);
    m_activeToken = nextToken();
    m_view.deadlineMonotonicMilliseconds = Private::saturatedDeadline(
        m_clock.nowMilliseconds(), m_timing.applyTimeoutMilliseconds);
    m_port.requestApply({.token = m_activeToken,
                         .scope = ApplyScope::ForwardCandidate,
                         .candidate = m_staged,
                         .survivingProperties = {}});
}

void Machine::requestRevert(const Display::TransactionReason reason)
{
    if (m_view.state == MachineState::Applying) {
        m_revertRequested = true;
        m_view.reason = reason;
        m_journal.reason = reason;
        static_cast<void>(m_port.storeJournal(m_journal));
        return;
    }
    beginRevert(reason);
}

CommandResult Machine::preview(const QString &transactionId)
{
    if (m_view.state != MachineState::Staged) {
        return rejected(CommandError::InvalidTransition);
    }
    if (!transactionMatches(transactionId)) {
        return rejected(CommandError::UnknownTransaction);
    }
    if (m_view.safety != SafetyState::Safe) {
        return rejected(CommandError::Locked);
    }
    Journal durableJournal = m_journal;
    durableJournal.phase = JournalPhase::Applying;
    if (!Private::journalMutationDurable(m_port.storeJournal(durableJournal))) {
        return rejected(CommandError::JournalFailure);
    }
    m_journal = std::move(durableJournal);
    m_view.journalActive = true;
    beginForwardApply();
    return accepted(true);
}

CommandResult Machine::confirm(const QString &transactionId)
{
    if (m_view.state != MachineState::AwaitingConfirmation) {
        return rejected(CommandError::InvalidTransition);
    }
    if (!transactionMatches(transactionId)) {
        return rejected(CommandError::UnknownTransaction);
    }
    if (!Private::journalMutationDurable(m_port.clearJournal())) {
        return rejected(CommandError::JournalFailure);
    }
    const Display::Snapshot confirmed = m_snapshot;
    finishReady(confirmed);
    return accepted(true);
}

CommandResult Machine::cancel(const QString &transactionId)
{
    if (!transactionMatches(transactionId)) {
        return rejected(CommandError::UnknownTransaction);
    }
    if (m_view.state == MachineState::Staged) {
        m_view.reason = Display::TransactionReason::Cancelled;
        clearTransaction();
        return accepted(true);
    }
    if (m_view.state == MachineState::SettlingTopology) {
        const bool actionChanged = !m_revertRequested && !m_abandonAfterSettle;
        if (actionChanged) {
            m_revertRequested = true;
            m_view.reason = Display::TransactionReason::Cancelled;
            m_journal.reason = Display::TransactionReason::Cancelled;
            static_cast<void>(m_port.storeJournal(m_journal));
        }
        return accepted(actionChanged);
    }
    if (Private::rollbackInProgress(m_view.state) || m_revertRequested) {
        return accepted(false);
    }
    if (!Private::activeState(m_view.state) || m_view.state == MachineState::Stuck) {
        return rejected(CommandError::InvalidTransition);
    }
    requestRevert(Display::TransactionReason::Cancelled);
    return accepted(true);
}

} // namespace QindaQt::DisplayTransaction
