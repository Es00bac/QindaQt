// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_transaction/transaction_machine.h>

#include <qindaqt/services/display_topology/topology.h>

#include "transaction_machine_p.h"

#include <QtCore/QSet>

#include <algorithm>

namespace QindaQt::DisplayTransaction
{
namespace
{

const Display::CandidateOutput *candidateOutput(const Display::Candidate &candidate,
                                                const QString &stableId)
{
    for (const Display::CandidateOutput &output : candidate.outputs) {
        if (output.stableId == stableId) {
            return &output;
        }
    }
    return nullptr;
}

const Display::Output *snapshotOutput(const Display::Snapshot &snapshot,
                                      const QString &stableId)
{
    for (const Display::Output &output : snapshot.outputs) {
        if (output.stableId == stableId) {
            return &output;
        }
    }
    return nullptr;
}

} // namespace

bool Machine::sameOutputSet(const Display::Snapshot &snapshot,
                            const Display::Candidate &candidate) const
{
    QSet<QString> snapshotIds;
    QSet<QString> candidateIds;
    for (const Display::Output &output : snapshot.outputs) {
        snapshotIds.insert(output.stableId);
    }
    for (const Display::CandidateOutput &output : candidate.outputs) {
        candidateIds.insert(output.stableId);
    }
    return snapshotIds == candidateIds;
}

bool Machine::sameOutputSet(const Display::Snapshot &left,
                            const Display::Snapshot &right) const
{
    QSet<QString> leftIds;
    QSet<QString> rightIds;
    for (const Display::Output &output : left.outputs) {
        leftIds.insert(output.stableId);
    }
    for (const Display::Output &output : right.outputs) {
        rightIds.insert(output.stableId);
    }
    return leftIds == rightIds;
}

QList<SurvivingOutputProperties> Machine::survivingProperties(
    const Display::Snapshot &snapshot) const
{
    QList<SurvivingOutputProperties> properties;
    for (const Display::Output &live : snapshot.outputs) {
        const Display::CandidateOutput *before = candidateOutput(m_preimage, live.stableId);
        // Disabled pre-image outputs have no restorable mode contract. The
        // survivor request intentionally never treats an empty mode as an
        // adapter wildcard or tries to re-enable an output after hotplug.
        if (before == nullptr || !before->enabled) {
            continue;
        }
        properties.push_back({.stableId = before->stableId,
                              .modeId = before->modeId,
                              .scale = before->scale,
                              .transform = before->transform});
    }
    std::sort(properties.begin(), properties.end(),
              [](const SurvivingOutputProperties &left,
                 const SurvivingOutputProperties &right) {
                  return left.stableId < right.stableId;
              });
    return properties;
}

bool Machine::snapshotMatchesSurvivingProperties(const Display::Snapshot &snapshot) const
{
    for (const SurvivingOutputProperties &properties : m_survivingProperties) {
        const Display::Output *live = snapshotOutput(snapshot, properties.stableId);
        if (live == nullptr || live->modeId != properties.modeId
            || live->scale != properties.scale || live->transform != properties.transform) {
            return false;
        }
    }
    return true;
}

ApplyRequest Machine::makeRevertRequest(const quint64 token) const
{
    if (!m_survivingProperties.isEmpty()) {
        return {.token = token,
                .scope = ApplyScope::SurvivingOutputProperties,
                .candidate = {},
                .survivingProperties = m_survivingProperties};
    }
    Display::Candidate candidate = m_preimage;
    candidate.baseEpoch = m_snapshot.serviceEpoch;
    candidate.baseRevision = m_snapshot.revision;
    return {.token = token,
            .scope = ApplyScope::FullPreimage,
            .candidate = std::move(candidate),
            .survivingProperties = {}};
}

void Machine::beginRevert(const Display::TransactionReason reason)
{
    m_activeToken = 0;
    m_revertRequested = false;
    m_abandonAfterSettle = false;
    m_cleanupOnlyStuck = false;
    m_view.reason = reason;
    m_view.deadlineMonotonicMilliseconds = 0;
    m_view.revertAttempt = 0;
    if (sameOutputSet(m_snapshot, m_preimage)) {
        m_survivingProperties.clear();
    } else if (m_survivingProperties.isEmpty()) {
        m_survivingProperties = survivingProperties(m_snapshot);
    }
    m_journal.phase = JournalPhase::Reverting;
    m_journal.reason = reason;
    m_journal.revertAttempt = 0;
    static_cast<void>(m_port.storeJournal(m_journal));
    issueRevertApply();
}

void Machine::issueRevertApply()
{
    if (m_view.revertAttempt >= kMaximumRevertAttempts) {
        enterStuck();
        return;
    }
    ++m_view.revertAttempt;
    m_journal.revertAttempt = m_view.revertAttempt;
    static_cast<void>(m_port.storeJournal(m_journal));
    m_activeToken = nextToken();
    setState(MachineState::RevertingApply);
    m_view.deadlineMonotonicMilliseconds = Private::saturatedDeadline(
        m_clock.nowMilliseconds(), m_timing.applyTimeoutMilliseconds);
    m_port.requestApply(makeRevertRequest(m_activeToken));
}

void Machine::scheduleRevertRetry()
{
    m_activeToken = 0;
    if (m_view.revertAttempt >= kMaximumRevertAttempts) {
        enterStuck();
        return;
    }
    const quint64 backoff = m_view.revertAttempt == 1
        ? m_timing.firstRevertBackoffMilliseconds
        : m_timing.secondRevertBackoffMilliseconds;
    setState(MachineState::RevertBackoff);
    m_view.deadlineMonotonicMilliseconds =
        Private::saturatedDeadline(m_clock.nowMilliseconds(), backoff);
}

void Machine::enterStuck(const bool cleanupOnly,
                         const Display::TransactionReason durableReason)
{
    m_activeToken = 0;
    m_revertRequested = false;
    m_cleanupOnlyStuck = cleanupOnly;
    setState(MachineState::Stuck);
    const Display::TransactionReason reason = cleanupOnly
        ? Display::TransactionReason::JournalFailure
        : Display::TransactionReason::RevertFailed;
    m_view.reason = reason;
    m_view.deadlineMonotonicMilliseconds = 0;
    m_view.journalActive = true;
    if (cleanupOnly) {
        m_view.revertAttempt = 0;
    }
    m_journal.phase = JournalPhase::Stuck;
    // AGENT-CONTRACT: A cleanup-only live view reports JournalFailure, but a
    // supplied durable reason remains the crash-recovery instruction.
    m_journal.reason = durableReason == Display::TransactionReason::None
        ? reason
        : durableReason;
    m_journal.revertAttempt = m_view.revertAttempt;
    static_cast<void>(m_port.storeJournal(m_journal));
}

CommandResult Machine::recover(const Journal &journal, const Display::Snapshot &snapshot,
                               const SafetyState safety)
{
    if (m_view.state != MachineState::Discovering) {
        return rejected(CommandError::InvalidTransition);
    }
    if (!isValidJournal(journal)) {
        return rejected(CommandError::InvalidJournal);
    }
    if (!validSnapshot(snapshot)) {
        return rejected(CommandError::InvalidSnapshot);
    }
    m_snapshot = snapshot;
    m_staged = journal.target;
    m_preimage = journal.preimage;
    m_journal = journal;
    m_view.safety = safety;
    m_view.currentRevision = snapshot.revision;
    m_view.transactionId = journal.transactionId;
    m_view.reason = Display::TransactionReason::Recovery;
    m_view.journalActive = true;
    if (journal.reason == Display::TransactionReason::ExternalChange) {
        m_view.reason = Display::TransactionReason::ExternalChange;
        if (!sameOutputSet(snapshot, m_preimage)) {
            m_abandonAfterSettle = true;
            setState(MachineState::SettlingTopology);
            m_view.deadlineMonotonicMilliseconds = 0;
            m_journal.phase = JournalPhase::Reverting;
            static_cast<void>(m_port.storeJournal(m_journal));
            return accepted(true, CommandError::ExternalChange);
        }
        if (!m_port.clearJournal()) {
            enterStuck(true);
            return accepted(true, CommandError::JournalFailure);
        }
        finishReady(snapshot);
        return accepted(true, CommandError::ExternalChange);
    }
    if (snapshotMatches(snapshot, m_preimage)) {
        if (!m_port.clearJournal()) {
            enterStuck(true);
            return accepted(true, CommandError::JournalFailure);
        }
        finishReady(snapshot);
        return accepted(true);
    }
    if (!sameOutputSet(snapshot, m_preimage)) {
        setState(MachineState::SettlingTopology);
        m_view.deadlineMonotonicMilliseconds = 0;
        m_survivingProperties.clear();
        m_journal.phase = JournalPhase::Reverting;
        m_journal.reason = Display::TransactionReason::Recovery;
        static_cast<void>(m_port.storeJournal(m_journal));
        return accepted(true);
    }
    if (snapshotMatches(snapshot, m_staged)) {
        beginRevert(Display::TransactionReason::Recovery);
        return accepted(true);
    }

    // A same-set layout matching neither durable endpoint is external truth.
    // Recovery may remove our stale journal but must not fight that intent.
    m_view.reason = Display::TransactionReason::ExternalChange;
    if (!m_port.clearJournal()) {
        enterStuck(true);
        return accepted(true, CommandError::JournalFailure);
    }
    finishReady(snapshot);
    return accepted(true, CommandError::ExternalChange);
}

CommandResult Machine::retryStuck()
{
    if (m_view.state != MachineState::Stuck) {
        return rejected(CommandError::InvalidTransition);
    }
    if (m_cleanupOnlyStuck || snapshotMatches(m_snapshot, m_preimage)) {
        if (!m_port.clearJournal()) {
            return accepted(false, CommandError::JournalFailure);
        }
        const Display::Snapshot current = m_snapshot;
        finishReady(current);
        return accepted(true);
    }
    m_view.revertAttempt = 0;
    m_journal.revertAttempt = 0;
    m_survivingProperties.clear();
    beginRevert(Display::TransactionReason::Recovery);
    return accepted(true);
}

} // namespace QindaQt::DisplayTransaction
