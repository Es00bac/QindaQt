// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_transaction/transaction_machine.h>

#include <qindaqt/services/display_topology/topology.h>

#include "transaction_machine_p.h"

namespace QindaQt::DisplayTransaction
{
namespace
{

bool followsCurrentLineage(const Display::Snapshot &snapshot,
                           const Display::Snapshot &current)
{
    // AGENT-CONTRACT: The adapter may redeliver unchanged truth after every
    // callback/deadline. Changed truth must advance the revision or a candidate
    // projected before that change can reuse the old fence and stage.
    return snapshot.serviceEpoch == current.serviceEpoch
        && (snapshot.revision > current.revision
            || (snapshot.revision == current.revision && snapshot == current));
}

} // namespace

bool Machine::snapshotMatches(const Display::Snapshot &snapshot,
                              const Display::Candidate &candidate) const
{
    return validSnapshot(snapshot)
        && snapshot.liveFingerprint == DisplayTopology::canonicalFingerprint(candidate);
}

CommandResult Machine::applyCompleted(const quint64 token, const ApplyOutcome outcome)
{
    if (token == 0 || token != m_activeToken) {
        return rejected(CommandError::CallbackOutOfOrder);
    }
    if (m_view.state == MachineState::Applying) {
        const bool rollbackWasRequested = m_revertRequested;
        m_activeToken = 0;
        m_view.deadlineMonotonicMilliseconds = Private::saturatedDeadline(
            m_clock.nowMilliseconds(), m_timing.observationTimeoutMilliseconds);
        if (rollbackWasRequested) {
            setState(MachineState::ResolvingUncertain);
            return accepted(true, outcome == ApplyOutcome::Rejected
                                      ? CommandError::ApplyRejected
                                      : outcome == ApplyOutcome::TransportUncertain
                                          ? CommandError::ApplyUncertain
                                          : CommandError::None);
        }
        if (outcome == ApplyOutcome::Applied) {
            setState(MachineState::Observing);
            return accepted(true);
        }
        setState(MachineState::ResolvingUncertain);
        m_view.reason = outcome == ApplyOutcome::Rejected
            ? Display::TransactionReason::ApplyRejected
            : Display::TransactionReason::TransportUncertain;
        return accepted(true, outcome == ApplyOutcome::Rejected
                                  ? CommandError::ApplyRejected
                                  : CommandError::ApplyUncertain);
    }
    if (m_view.state == MachineState::RevertingApply) {
        m_activeToken = 0;
        if (outcome == ApplyOutcome::Applied) {
            setState(MachineState::RevertingObserve);
            m_view.deadlineMonotonicMilliseconds = Private::saturatedDeadline(
                m_clock.nowMilliseconds(), m_timing.observationTimeoutMilliseconds);
        } else {
            scheduleRevertRetry();
        }
        return accepted(true, outcome == ApplyOutcome::Applied
                                  ? CommandError::None
                                  : CommandError::RevertFailed);
    }
    return rejected(CommandError::CallbackOutOfOrder);
}

CommandResult Machine::observedSnapshot(const Display::Snapshot &snapshot)
{
    if (!validSnapshot(snapshot)) {
        return rejected(CommandError::InvalidSnapshot);
    }
    if (m_view.state == MachineState::Stuck) {
        const bool changed = snapshot != m_snapshot;
        m_snapshot = snapshot;
        m_view.currentRevision = snapshot.revision;
        m_survivingProperties.clear();
        const bool becameCleanupOnly = !m_cleanupOnlyStuck
            && snapshotMatches(snapshot, m_preimage);
        if (becameCleanupOnly) {
            enterStuck(true);
        }
        return accepted(changed || becameCleanupOnly,
                        m_cleanupOnlyStuck ? CommandError::JournalFailure
                                          : CommandError::RevertFailed);
    }
    if (m_view.state == MachineState::SettlingTopology) {
        return topologyChanged(snapshot);
    }
    if (Private::activeState(m_view.state) && !sameOutputSet(snapshot, m_snapshot)) {
        return topologyChanged(snapshot);
    }
    if (m_view.state == MachineState::Ready) {
        if (!followsCurrentLineage(snapshot, m_snapshot)) {
            return rejected(CommandError::InvalidSnapshot);
        }
        const bool changed = snapshot != m_snapshot;
        m_snapshot = snapshot;
        m_view.currentRevision = snapshot.revision;
        return accepted(changed);
    }
    if (m_view.state == MachineState::Observing) {
        const bool changed = snapshot != m_snapshot;
        m_snapshot = snapshot;
        m_view.currentRevision = snapshot.revision;
        if (snapshotMatches(snapshot, m_staged)) {
            m_journal.phase = JournalPhase::AwaitingConfirmation;
            if (!m_port.storeJournal(m_journal)) {
                beginRevert(Display::TransactionReason::JournalFailure);
                return accepted(true, CommandError::JournalFailure);
            }
            setState(MachineState::AwaitingConfirmation);
            m_view.deadlineMonotonicMilliseconds = Private::saturatedDeadline(
                m_clock.nowMilliseconds(), m_timing.confirmationTimeoutMilliseconds);
            return accepted(true);
        }
        if (snapshotMatches(snapshot, m_preimage)) {
            if (!m_port.clearJournal()) {
                enterStuck(true);
                return accepted(true, CommandError::JournalFailure);
            }
            m_view.reason = Display::TransactionReason::ApplyRejected;
            finishReady(snapshot);
            return accepted(true, CommandError::ApplyRejected);
        }
        return accepted(changed, CommandError::ObservationMismatch);
    }
    if (m_view.state == MachineState::ResolvingUncertain) {
        const Display::TransactionReason uncertaintyReason = m_view.reason;
        const bool changed = snapshot != m_snapshot;
        m_snapshot = snapshot;
        m_view.currentRevision = snapshot.revision;
        if (snapshotMatches(snapshot, m_preimage)) {
            if (!m_port.clearJournal()) {
                enterStuck(true);
                return accepted(true, CommandError::JournalFailure);
            }
            finishReady(snapshot);
            return accepted(true, uncertaintyReason == Display::TransactionReason::ApplyRejected
                                      ? CommandError::ApplyRejected
                                      : CommandError::ApplyUncertain);
        }
        if (snapshotMatches(snapshot, m_staged)) {
            beginRevert(uncertaintyReason);
            return accepted(true, CommandError::ApplyUncertain);
        }
        return accepted(changed, CommandError::ObservationMismatch);
    }
    if (m_view.state == MachineState::RevertingObserve) {
        const bool changed = snapshot != m_snapshot;
        m_snapshot = snapshot;
        m_view.currentRevision = snapshot.revision;
        const bool matched = m_survivingProperties.isEmpty()
            ? snapshotMatches(snapshot, m_preimage)
            : snapshotMatchesSurvivingProperties(snapshot);
        if (!matched) {
            return accepted(changed, CommandError::ObservationMismatch);
        }
        if (!m_port.clearJournal()) {
            enterStuck(true);
            return accepted(true, CommandError::JournalFailure);
        }
        finishReady(snapshot);
        return accepted(true);
    }
    if (m_view.state == MachineState::AwaitingConfirmation) {
        if (snapshot.liveFingerprint != m_snapshot.liveFingerprint) {
            return externalIntentObserved(snapshot);
        }
        const bool changed = snapshot != m_snapshot;
        m_snapshot = snapshot;
        m_view.currentRevision = snapshot.revision;
        return accepted(changed);
    }
    return rejected(CommandError::CallbackOutOfOrder);
}

CommandResult Machine::externalIntentObserved(const Display::Snapshot &snapshot)
{
    if (!validSnapshot(snapshot)) {
        return rejected(CommandError::InvalidSnapshot);
    }
    if (m_view.state == MachineState::Discovering) {
        return rejected(CommandError::InvalidTransition);
    }
    if (m_view.state == MachineState::SettlingTopology
        || (Private::activeState(m_view.state)
            && m_view.state != MachineState::Stuck
            && !sameOutputSet(snapshot, m_snapshot))) {
        const CommandResult topologyResult = topologyChanged(snapshot);
        if (!topologyResult.accepted) {
            return topologyResult;
        }
        const bool actionChanged = !m_abandonAfterSettle
            || m_view.reason != Display::TransactionReason::ExternalChange;
        m_abandonAfterSettle = true;
        m_revertRequested = false;
        m_view.reason = Display::TransactionReason::ExternalChange;
        m_journal.reason = Display::TransactionReason::ExternalChange;
        static_cast<void>(m_port.storeJournal(m_journal));
        return accepted(topologyResult.stateChanged || actionChanged,
                        CommandError::ExternalChange);
    }
    if (m_view.state == MachineState::Ready) {
        if (!followsCurrentLineage(snapshot, m_snapshot)) {
            return rejected(CommandError::InvalidSnapshot);
        }
        const bool changed = snapshot != m_snapshot;
        m_snapshot = snapshot;
        m_view.currentRevision = snapshot.revision;
        return accepted(changed);
    }
    if (m_view.state == MachineState::Stuck) {
        m_snapshot = snapshot;
        m_view.currentRevision = snapshot.revision;
        m_view.reason = Display::TransactionReason::ExternalChange;
        m_cleanupOnlyStuck = true;
        m_survivingProperties.clear();
        if (m_view.journalActive) {
            m_journal.reason = Display::TransactionReason::ExternalChange;
            static_cast<void>(m_port.storeJournal(m_journal));
            if (!m_port.clearJournal()) {
                enterStuck(true, Display::TransactionReason::ExternalChange);
                return accepted(true, CommandError::JournalFailure);
            }
        }
        finishReady(snapshot);
        return accepted(true, CommandError::ExternalChange);
    }
    m_activeToken = 0;
    m_snapshot = snapshot;
    m_view.currentRevision = snapshot.revision;
    m_view.reason = Display::TransactionReason::ExternalChange;
    if (m_view.journalActive) {
        // AGENT-GUARD: Persist abandon before clearing. If clear fails or the
        // process dies between calls, recovery must not replay the pre-image
        // over external truth.
        m_journal.reason = Display::TransactionReason::ExternalChange;
        static_cast<void>(m_port.storeJournal(m_journal));
        if (!m_port.clearJournal()) {
            enterStuck(true, Display::TransactionReason::ExternalChange);
            return accepted(true, CommandError::JournalFailure);
        }
    }
    finishReady(snapshot);
    return accepted(true, CommandError::ExternalChange);
}

CommandResult Machine::topologyChanged(const Display::Snapshot &snapshot)
{
    if (!validSnapshot(snapshot)) {
        return rejected(CommandError::InvalidSnapshot);
    }
    if (m_view.state == MachineState::Discovering) {
        return rejected(CommandError::InvalidTransition);
    }
    if (m_view.state == MachineState::Ready) {
        if (!followsCurrentLineage(snapshot, m_snapshot)) {
            return rejected(CommandError::InvalidSnapshot);
        }
        const bool changed = snapshot != m_snapshot;
        m_snapshot = snapshot;
        m_view.currentRevision = snapshot.revision;
        return accepted(changed);
    }
    if (m_view.state == MachineState::Staged) {
        m_snapshot = snapshot;
        m_view.currentRevision = snapshot.revision;
        m_view.reason = Display::TransactionReason::TopologyChanged;
        clearTransaction();
        return accepted(true, CommandError::TopologyChanged);
    }
    if (m_view.state == MachineState::Stuck) {
        const bool changed = snapshot != m_snapshot;
        m_snapshot = snapshot;
        m_view.currentRevision = snapshot.revision;
        m_survivingProperties.clear();
        const bool becameCleanupOnly = !m_cleanupOnlyStuck
            && snapshotMatches(snapshot, m_preimage);
        if (becameCleanupOnly) {
            enterStuck(true);
        }
        return accepted(changed || becameCleanupOnly,
                        m_cleanupOnlyStuck ? CommandError::JournalFailure
                                          : CommandError::RevertFailed);
    }
    const bool wasSettling = m_view.state == MachineState::SettlingTopology;
    const bool snapshotChanged = snapshot != m_snapshot;
    m_activeToken = 0;
    m_snapshot = snapshot;
    m_view.currentRevision = snapshot.revision;
    m_view.deadlineMonotonicMilliseconds = 0;
    m_survivingProperties.clear();
    setState(MachineState::SettlingTopology);
    m_journal.phase = JournalPhase::Reverting;
    if (!wasSettling) {
        m_abandonAfterSettle = false;
        m_revertRequested = false;
        m_view.reason = Display::TransactionReason::TopologyChanged;
        m_journal.reason = Display::TransactionReason::TopologyChanged;
    }
    static_cast<void>(m_port.storeJournal(m_journal));
    return accepted(!wasSettling || snapshotChanged, CommandError::TopologyChanged);
}

CommandResult Machine::topologySettled(const Display::Snapshot &snapshot)
{
    if (m_view.state != MachineState::SettlingTopology) {
        return rejected(CommandError::InvalidTransition);
    }
    if (!validSnapshot(snapshot)) {
        return rejected(CommandError::InvalidSnapshot);
    }
    m_snapshot = snapshot;
    m_view.currentRevision = snapshot.revision;
    if (m_abandonAfterSettle) {
        if (!m_port.clearJournal()) {
            enterStuck(true);
            return accepted(true, CommandError::JournalFailure);
        }
        finishReady(snapshot);
        return accepted(true, CommandError::ExternalChange);
    }
    const bool originalSetRestored = sameOutputSet(snapshot, m_preimage);
    m_survivingProperties = originalSetRestored ? QList<SurvivingOutputProperties>{}
                                                : survivingProperties(snapshot);
    if (originalSetRestored) {
        if (snapshotMatches(snapshot, m_preimage)) {
            if (!m_port.clearJournal()) {
                enterStuck(true);
                return accepted(true, CommandError::JournalFailure);
            }
            finishReady(snapshot);
            return accepted(true, CommandError::TopologyChanged);
        }
        beginRevert(m_view.reason);
        return accepted(true, CommandError::TopologyChanged);
    }
    if (m_survivingProperties.isEmpty()) {
        if (!m_port.clearJournal()) {
            enterStuck(true);
            return accepted(true, CommandError::JournalFailure);
        }
        finishReady(snapshot);
        return accepted(true, CommandError::TopologyChanged);
    }
    if (snapshotMatchesSurvivingProperties(snapshot)) {
        if (!m_port.clearJournal()) {
            enterStuck(true);
            return accepted(true, CommandError::JournalFailure);
        }
        finishReady(snapshot);
        return accepted(true, CommandError::TopologyChanged);
    }
    beginRevert(m_view.reason);
    return accepted(true, CommandError::TopologyChanged);
}

CommandResult Machine::safetyChanged(const SafetyState safety)
{
    const bool changedSafety = safety != m_view.safety;
    m_view.safety = safety;
    if (safety == SafetyState::Safe || m_view.state == MachineState::Ready
        || m_view.state == MachineState::Discovering) {
        return accepted(changedSafety);
    }
    if (m_view.state == MachineState::Staged) {
        m_view.reason = Display::TransactionReason::Locked;
        clearTransaction();
        return accepted(true, CommandError::Locked);
    }
    if (m_view.state == MachineState::Stuck) {
        return accepted(changedSafety, m_cleanupOnlyStuck ? CommandError::JournalFailure
                                                         : CommandError::RevertFailed);
    }
    if (m_view.state == MachineState::SettlingTopology) {
        const bool actionChanged = !m_revertRequested && !m_abandonAfterSettle;
        if (actionChanged) {
            m_revertRequested = true;
            m_view.reason = Display::TransactionReason::Locked;
            m_journal.reason = Display::TransactionReason::Locked;
            static_cast<void>(m_port.storeJournal(m_journal));
        }
        return accepted(changedSafety || actionChanged, CommandError::Locked);
    }
    if (Private::rollbackInProgress(m_view.state) || m_revertRequested) {
        return accepted(changedSafety, CommandError::Locked);
    }
    requestRevert(Display::TransactionReason::Locked);
    return accepted(true, CommandError::Locked);
}

CommandResult Machine::prepareForSuspend()
{
    if (m_view.state == MachineState::Discovering || m_view.state == MachineState::Ready) {
        return accepted(false);
    }
    if (m_view.state == MachineState::Staged) {
        m_view.reason = Display::TransactionReason::Suspend;
        clearTransaction();
        return accepted(true, CommandError::Suspend);
    }
    if (m_view.state == MachineState::Stuck) {
        return accepted(false, m_cleanupOnlyStuck ? CommandError::JournalFailure
                                                 : CommandError::RevertFailed);
    }
    if (m_view.state == MachineState::SettlingTopology) {
        const bool actionChanged = !m_revertRequested && !m_abandonAfterSettle;
        if (actionChanged) {
            m_revertRequested = true;
            m_view.reason = Display::TransactionReason::Suspend;
            m_journal.reason = Display::TransactionReason::Suspend;
            static_cast<void>(m_port.storeJournal(m_journal));
        }
        return accepted(actionChanged, CommandError::Suspend);
    }
    if (Private::rollbackInProgress(m_view.state) || m_revertRequested) {
        return accepted(false, CommandError::Suspend);
    }
    requestRevert(Display::TransactionReason::Suspend);
    return accepted(true);
}

CommandResult Machine::tick()
{
    const quint64 now = m_clock.nowMilliseconds();
    if (m_view.deadlineMonotonicMilliseconds == 0
        || now < m_view.deadlineMonotonicMilliseconds) {
        return accepted(false);
    }
    // AGENT-GUARD: Every state that installs a non-zero deadline must have a
    // progress branch here. Omitting one can strand a durable preimage forever.
    if (m_view.state == MachineState::Applying) {
        m_activeToken = 0;
        setState(MachineState::ResolvingUncertain);
        if (!m_revertRequested) {
            m_view.reason = Display::TransactionReason::ApplyTimeout;
        }
        m_view.deadlineMonotonicMilliseconds = Private::saturatedDeadline(
            now, m_timing.observationTimeoutMilliseconds);
        return accepted(true, CommandError::ApplyUncertain);
    }
    if (m_view.state == MachineState::ResolvingUncertain) {
        beginRevert(m_view.reason);
        return accepted(true, CommandError::ApplyUncertain);
    }
    if (m_view.state == MachineState::Observing) {
        beginRevert(Display::TransactionReason::ObservationTimeout);
        return accepted(true, CommandError::ObservationTimeout);
    }
    if (m_view.state == MachineState::AwaitingConfirmation) {
        beginRevert(Display::TransactionReason::ConfirmationDeadline);
        return accepted(true);
    }
    if (m_view.state == MachineState::RevertingApply) {
        scheduleRevertRetry();
        return accepted(true, CommandError::RevertFailed);
    }
    if (m_view.state == MachineState::RevertingObserve) {
        scheduleRevertRetry();
        return accepted(true, CommandError::RevertFailed);
    }
    if (m_view.state == MachineState::RevertBackoff) {
        issueRevertApply();
        return accepted(true);
    }
    return accepted(false);
}

} // namespace QindaQt::DisplayTransaction
