// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_history.h>

#include <limits>

#include <qindaqt/services/clipboard_model/clipboard_media.h>

#include "clipboard_history_p.h"

namespace QindaQt::Services::ClipboardModel {

using HistoryDetail::entryFingerprint;

ClipboardHistoryModel::ClipboardHistoryModel(HistoryLimits limits)
    : ClipboardHistoryModel(limits, HistoryCounters {})
{
}

ClipboardHistoryModel::ClipboardHistoryModel(HistoryLimits limits,
                                             const HistoryCounters &counters)
    : m_limits(sanitizeLimits(limits))
{
    const HistoryCounters sanitized = sanitizeCounters(counters);
    m_generation = sanitized.generation;
    m_nextSerial = sanitized.nextSerial;
    m_revision = sanitized.revision;
    // AGENT-GUARD: limits and counters are sanitized, not asserted, so the
    // protocol ceilings and non-zero lineage hold in Release builds too. No
    // code path may widen m_limits or reset a lineage counter to zero after
    // construction; exhaustion must latch through the flags instead.
}

ClipboardHistoryModel::Gate ClipboardHistoryModel::gateOperation(
    quint32 expectedGeneration) const noexcept
{
    Gate gate;
    if (!m_historyEnabled) {
        gate.error = ClipboardError::HistoryDisabled;
        gate.refused = true;
        return gate;
    }
    if (m_privacy != PrivacyState::Allowed) {
        gate.error = ClipboardError::PrivacyDenied;
        gate.refused = true;
        return gate;
    }
    if (expectedGeneration != m_generation) {
        gate.error = ClipboardError::StaleGeneration;
        gate.refused = true;
        return gate;
    }
    if (m_generationExhausted || revisionAtCeiling()) {
        // Fail closed: refusing content operations is always safe, while
        // wrapping a fixed-width lineage would forge duplicate identities.
        gate.error = ClipboardError::LineageExhausted;
        gate.refused = true;
        return gate;
    }
    return gate;
}

bool ClipboardHistoryModel::revisionAtCeiling() const noexcept
{
    return m_revision == std::numeric_limits<quint64>::max();
}

// AGENT-GUARD: only callable after gateOperation() has accepted the caller —
// the gate refuses LineageExhausted at the ceiling, so this increment can
// never wrap. Bumping without a passed gate would forge a revision beyond
// the representable lineage.
void ClipboardHistoryModel::bumpRevision() noexcept
{
    m_revision += 1;
}

void ClipboardHistoryModel::setHistoryEnabled(bool enabled)
{
    if (m_historyEnabled == enabled) {
        return;
    }
    m_historyEnabled = enabled;
    if (!enabled) {
        purgeAndRaiseGeneration();
    }
}

void ClipboardHistoryModel::setPrivacyAllowed(bool allowed)
{
    if (m_privacy == (allowed ? PrivacyState::Allowed : PrivacyState::Denied)) {
        return;
    }
    m_privacy = allowed ? PrivacyState::Allowed : PrivacyState::Denied;
    if (!allowed) {
        purgeAndRaiseGeneration();
    }
}

qsizetype ClipboardHistoryModel::indexOf(EntryId id) const noexcept
{
    if (!id.isValid()) {
        return -1;
    }
    for (qsizetype index = 0; index < m_entries.size(); ++index) {
        if (m_entries.at(index).descriptor.id == id) {
            return index;
        }
    }
    return -1;
}

void ClipboardHistoryModel::purgeAndRaiseGeneration()
{
    m_entries.clear();
    m_totalPayloadBytes = 0;
    m_nextSerial = 1;
    m_serialExhausted = false;
    // AGENT-GUARD: destroying content is unconditional — a privacy purge can
    // never be refused — but the counter itself is fail-closed. At the
    // 32-bit ceiling the generation stays pinned and every further content
    // operation refuses with LineageExhausted instead of wrapping to zero
    // and forging pre-purge identities.
    if (m_generation == std::numeric_limits<quint32>::max()) {
        m_generationExhausted = true;
    } else {
        m_generation += 1;
    }
}

ClipboardEntryDescriptor ClipboardHistoryModel::makeDescriptor(const ClipboardValue &value,
                                                               const QString &sourceLabel,
                                                               quint64 tick) const
{
    ClipboardEntryDescriptor descriptor;
    descriptor.sourceLabel = sanitizeSourceLabel(sourceLabel, m_limits.maxSourceLabelCodeUnits);
    descriptor.admittedTick = tick;
    descriptor.lastUsedTick = tick;
    const HistoryDetail::DerivedPreview preview =
        HistoryDetail::derivePreview(value.formats, m_limits.maxPreviewCodeUnits);
    descriptor.preview = preview.preview;
    descriptor.previewTruncated = preview.truncated;
    descriptor.fingerprint = entryFingerprint(value.formats);
    for (const ClipboardFormat &format : value.formats) {
        descriptor.formats.append(FormatInfo { format.mediaType, format.payload.size() });
    }
    return descriptor;
}

HistorySnapshot ClipboardHistoryModel::snapshot() const
{
    HistorySnapshot snapshot;
    snapshot.generation = m_generation;
    snapshot.revision = m_revision;
    snapshot.historyEnabled = m_historyEnabled;
    snapshot.privacyAllowed = m_privacy == PrivacyState::Allowed;
    if (!m_historyEnabled || m_privacy != PrivacyState::Allowed) {
        // Withholding is expressed by empty content plus the flags; no
        // metadata — not even aggregate byte totals — is disclosed while
        // history is disabled or privacy denied.
        return snapshot;
    }
    snapshot.totalPayloadBytes = m_totalPayloadBytes;
    snapshot.entries.reserve(m_entries.size());
    for (const Entry &entry : m_entries) {
        snapshot.entries.append(entry.descriptor);
    }
    return snapshot;
}

} // namespace QindaQt::Services::ClipboardModel
