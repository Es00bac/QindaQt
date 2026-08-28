// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_history.h>

#include <qindaqt/services/clipboard_model/clipboard_media.h>

#include <QtCore/QSet>

#include "clipboard_history_p.h"

namespace QindaQt::Services::ClipboardModel {

namespace {

using HistoryDetail::entryFingerprint;
using HistoryDetail::totalFormatBytes;

// Validates and canonicalizes one admitted value against instance limits.
// Refusal precedence within the value is fixed: shape, media canonicality,
// classification, then size, so a hostile value always produces the same
// error class regardless of which rule a human reader would notice first.
ValueValidation validateValue(const ClipboardValue &value, const HistoryLimits &limits)
{
    ValueValidation result;
    if (value.formats.isEmpty()) {
        result.error = ClipboardError::EmptyValue;
        return result;
    }
    if (value.formats.size() > limits.maxFormatsPerItem) {
        result.error = ClipboardError::TooManyFormats;
        return result;
    }
    bool hasPayload = false;
    QSet<QString> seenMedia;
    result.canonicalFormats.reserve(value.formats.size());
    for (const ClipboardFormat &format : value.formats) {
        const MediaCanonicalization canonical =
            canonicalizeMediaType(format.mediaType, limits.maxMediaTypeLength);
        if (!canonical.accepted()) {
            result.error = ClipboardError::MediaTypeRejected;
            result.offendingMediaType = format.mediaType;
            return result;
        }
        if (seenMedia.contains(canonical.canonical)) {
            result.error = ClipboardError::DuplicateFormat;
            result.offendingMediaType = canonical.canonical;
            return result;
        }
        seenMedia.insert(canonical.canonical);
        switch (classifyMediaType(canonical.canonical)) {
        case MediaClass::Sensitive:
            result.error = ClipboardError::SensitiveRefused;
            result.offendingMediaType = canonical.canonical;
            return result;
        case MediaClass::OneTime:
            result.error = ClipboardError::OneTimeRefused;
            result.offendingMediaType = canonical.canonical;
            return result;
        case MediaClass::NonStorable:
            result.error = ClipboardError::NonStorableRefused;
            result.offendingMediaType = canonical.canonical;
            return result;
        case MediaClass::Storable:
            break;
        }
        ClipboardFormat canonicalFormat;
        canonicalFormat.mediaType = canonical.canonical;
        canonicalFormat.payload = format.payload;
        if (!canonicalFormat.payload.isEmpty()) {
            hasPayload = true;
        }
        result.totalPayloadBytes += canonicalFormat.payload.size();
        result.canonicalFormats.append(canonicalFormat);
    }
    if (!hasPayload) {
        result.error = ClipboardError::EmptyValue;
        return result;
    }
    if (result.totalPayloadBytes > limits.maxItemPayloadBytes) {
        result.error = ClipboardError::OversizedValue;
        result.offendingMediaType.clear();
        return result;
    }
    return result;
}

} // namespace

AdmitOutcome ClipboardHistoryModel::admit(const ClipboardValue &value,
                                          quint32 expectedGeneration,
                                          const QString &sourceLabel,
                                          quint64 tick)
{
    AdmitOutcome outcome;
    const Gate gate = gateOperation(expectedGeneration);
    if (gate.refused) {
        outcome.error = gate.error;
        return outcome;
    }
    const ValueValidation validation = validateValue(value, m_limits);
    if (!validation.accepted()) {
        outcome.error = validation.error;
        return outcome;
    }

    const QByteArray fingerprint = entryFingerprint(validation.canonicalFormats);
    for (qsizetype index = 0; index < m_entries.size(); ++index) {
        if (m_entries.at(index).descriptor.fingerprint != fingerprint) {
            continue;
        }
        // Dedup: the item already exists. Move it to the most-recent
        // position, refresh caller-visible metadata, and keep the original
        // identity and pin state so a pinned item stays pinned.
        Entry entry = m_entries.takeAt(index);
        entry.descriptor.admittedTick = tick;
        entry.descriptor.lastUsedTick = tick;
        entry.descriptor.sourceLabel =
            sanitizeSourceLabel(sourceLabel, m_limits.maxSourceLabelCodeUnits);
        m_entries.prepend(entry);
        m_revision += 1;
        outcome.error = ClipboardError::None;
        outcome.entry = entry.descriptor;
        return outcome;
    }

    const qint64 incomingBytes = validation.totalPayloadBytes;
    // Capacity: refuse before evicting anything if the pinned set alone can
    // never make room, so a refusal never mutates the history.
    while (m_entries.size() >= m_limits.maxEntries
           || m_totalPayloadBytes + incomingBytes > m_limits.maxTotalPayloadBytes) {
        const qsizetype victim = lastUnpinnedIndex();
        if (victim < 0) {
            outcome.error = ClipboardError::CapacityRefused;
            return outcome;
        }
        m_totalPayloadBytes -= totalFormatBytes(m_entries.at(victim).value.formats);
        m_entries.removeAt(victim);
    }

    Entry entry;
    entry.value.formats = validation.canonicalFormats;
    entry.descriptor = makeDescriptor(entry.value, sourceLabel, tick);
    entry.descriptor.id = EntryId { m_generation, m_nextSerial };
    m_nextSerial += 1;
    m_totalPayloadBytes += incomingBytes;
    m_entries.prepend(entry);
    m_revision += 1;
    outcome.entry = entry.descriptor;
    return outcome;
}

PromoteOutcome ClipboardHistoryModel::promote(EntryId id, quint32 expectedGeneration, quint64 tick)
{
    PromoteOutcome outcome;
    const Gate gate = gateOperation(expectedGeneration);
    if (gate.refused) {
        outcome.error = gate.error;
        return outcome;
    }
    const qsizetype index = indexOf(id);
    if (index < 0) {
        outcome.error = ClipboardError::UnknownEntry;
        return outcome;
    }
    Entry entry = m_entries.takeAt(index);
    entry.descriptor.lastUsedTick = tick;
    m_entries.prepend(entry);
    m_revision += 1;
    outcome.value = entry.value;
    outcome.entry = entry.descriptor;
    return outcome;
}

MutationOutcome ClipboardHistoryModel::removeEntry(EntryId id, quint32 expectedGeneration)
{
    MutationOutcome outcome;
    const Gate gate = gateOperation(expectedGeneration);
    if (gate.refused) {
        outcome.error = gate.error;
        return outcome;
    }
    const qsizetype index = indexOf(id);
    if (index < 0) {
        outcome.error = ClipboardError::UnknownEntry;
        return outcome;
    }
    const Entry removed = m_entries.takeAt(index);
    m_totalPayloadBytes -= totalFormatBytes(removed.value.formats);
    m_revision += 1;
    return outcome;
}

MutationOutcome ClipboardHistoryModel::setPinned(EntryId id, bool pinned, quint32 expectedGeneration)
{
    MutationOutcome outcome;
    const Gate gate = gateOperation(expectedGeneration);
    if (gate.refused) {
        outcome.error = gate.error;
        return outcome;
    }
    const qsizetype index = indexOf(id);
    if (index < 0) {
        outcome.error = ClipboardError::UnknownEntry;
        return outcome;
    }
    if (m_entries.at(index).descriptor.pinned == pinned) {
        // Re-stating a pin is a no-op; nothing changed, so no revision.
        return outcome;
    }
    if (pinned) {
        int pinnedCount = 0;
        for (const Entry &entry : m_entries) {
            pinnedCount += entry.descriptor.pinned ? 1 : 0;
        }
        if (pinnedCount >= m_limits.maxPinnedEntries) {
            outcome.error = ClipboardError::PinnedLimitReached;
            return outcome;
        }
    }
    m_entries[index].descriptor.pinned = pinned;
    m_revision += 1;
    return outcome;
}

MutationOutcome ClipboardHistoryModel::clear(ClearScope scope, quint32 expectedGeneration)
{
    MutationOutcome outcome;
    const Gate gate = gateOperation(expectedGeneration);
    if (gate.refused) {
        outcome.error = gate.error;
        return outcome;
    }
    QList<Entry> kept;
    kept.reserve(m_entries.size());
    qint64 removedBytes = 0;
    for (Entry &entry : m_entries) {
        const bool keep = scope == ClearScope::UnpinnedOnly && entry.descriptor.pinned;
        if (keep) {
            kept.append(entry);
            continue;
        }
        removedBytes += totalFormatBytes(entry.value.formats);
    }
    if (kept.size() == m_entries.size()) {
        // Clearing nothing is a successful no-op that must not advance the
        // revision, matching the "refusals and no-ops change nothing" rule.
        return outcome;
    }
    m_entries = kept;
    m_totalPayloadBytes -= removedBytes;
    m_revision += 1;
    return outcome;
}

} // namespace QindaQt::Services::ClipboardModel
