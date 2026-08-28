// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_history.h>

#include <qindaqt/services/clipboard_model/clipboard_media.h>

#include <QtGlobal>

#include "clipboard_history_p.h"

namespace QindaQt::Services::ClipboardModel {

using HistoryDetail::derivePreview;
using HistoryDetail::entryFingerprint;

ClipboardHistoryModel::ClipboardHistoryModel(HistoryLimits limits)
    : m_limits(limits)
{
    // AGENT-GUARD: every capacity and clamping decision below assumes the
    // limits passed protocol validation; an invalid instance would let
    // oversized values into the volatile history.
    Q_ASSERT(isValidLimits(m_limits));
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
    return gate;
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

qsizetype ClipboardHistoryModel::lastUnpinnedIndex() const noexcept
{
    for (qsizetype index = m_entries.size() - 1; index >= 0; --index) {
        if (!m_entries.at(index).descriptor.pinned) {
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
    // AGENT-GUARD: this is the only place the generation changes. Every
    // stale-handle and stale-decision guarantee (including stale admission
    // refusal) depends on purges being exactly one generation bump wide.
    m_generation += 1;
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
