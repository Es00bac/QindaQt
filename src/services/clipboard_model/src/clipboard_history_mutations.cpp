// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_history.h>

#include <limits>

#include <qindaqt/services/clipboard_model/clipboard_media.h>

#include <QtCore/QSet>

#include "clipboard_history_p.h"

namespace QindaQt::Services::ClipboardModel {

namespace {

using HistoryDetail::entryFingerprint;
using HistoryDetail::totalFormatBytes;

// Canonical media names and size truth for one incoming value. The measure
// pass fills this without copying any payload byte; the copy pass runs only
// after every ceiling has been enforced, so a hostile value can never make
// the model allocate or duplicate before it is refused.
struct MeasuredValue {
    QList<ClipboardFormat> canonicalFormats;
    QList<MediaClass> classes;
    qint64 totalPayloadBytes = 0;
    bool hasPayload = false;
};

// Validates and canonicalizes one admitted value against instance limits.
// Refusal precedence within the value is fixed and order-independent:
// shape (EmptyValue, TooManyFormats), media canonicality, duplicate names,
// accumulated class truth (SensitiveRefused beats OneTimeRefused beats
// NonStorableRefused regardless of which format appears first), then size.
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

    // Measure pass: canonical names, duplicate detection, and byte totals
    // only. Media strings are bounded by maxMediaTypeLength and payloads are
    // not touched.
    MeasuredValue measured;
    QSet<QString> seenMedia;
    measured.classes.reserve(value.formats.size());
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
        measured.classes.append(classifyMediaType(canonical.canonical));
        ClipboardFormat canonicalFormat;
        canonicalFormat.mediaType = canonical.canonical;
        canonicalFormat.payload = format.payload;
        if (!canonicalFormat.payload.isEmpty()) {
            measured.hasPayload = true;
        }
        measured.totalPayloadBytes += canonicalFormat.payload.size();
        measured.canonicalFormats.append(canonicalFormat);
    }
    if (!measured.hasPayload) {
        result.error = ClipboardError::EmptyValue;
        return result;
    }

    // Class pass: accumulate across every format so producer ordering can
    // never change which policy refusal an identical value produces.
    bool sensitive = false;
    bool oneTime = false;
    bool nonStorable = false;
    for (const MediaClass classified : measured.classes) {
        switch (classified) {
        case MediaClass::Sensitive:
            sensitive = true;
            break;
        case MediaClass::OneTime:
            oneTime = true;
            break;
        case MediaClass::NonStorable:
            nonStorable = true;
            break;
        case MediaClass::Storable:
            break;
        }
    }
    if (sensitive) {
        result.error = ClipboardError::SensitiveRefused;
        return result;
    }
    if (oneTime) {
        result.error = ClipboardError::OneTimeRefused;
        return result;
    }
    if (nonStorable) {
        result.error = ClipboardError::NonStorableRefused;
        return result;
    }

    // Size pass: still before any payload copying for the caller.
    if (measured.totalPayloadBytes > limits.maxItemPayloadBytes) {
        result.error = ClipboardError::OversizedValue;
        return result;
    }

    result.totalPayloadBytes = measured.totalPayloadBytes;
    result.canonicalFormats = measured.canonicalFormats;
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
    if (m_serialExhausted) {
        outcome.error = ClipboardError::LineageExhausted;
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
        bumpRevision();
        outcome.error = ClipboardError::None;
        outcome.entry = entry.descriptor;
        return outcome;
    }

    const qint64 incomingBytes = validation.totalPayloadBytes;
    // AGENT-GUARD: capacity is decided on shadow state before any mutation.
    // Scanning victims here and only then removing them is what keeps a
    // CapacityRefused admission fully atomic — a refusal must leave entries,
    // byte totals, and revision exactly as they were, even when pins block
    // part of the needed space.
    qsizetype victimScan = m_entries.size();
    qsizetype entriesAfter = m_entries.size();
    qint64 bytesAfter = m_totalPayloadBytes;
    QList<qsizetype> victims;
    while (entriesAfter + 1 > m_limits.maxEntries
           || bytesAfter + incomingBytes > m_limits.maxTotalPayloadBytes) {
        qsizetype victim = -1;
        for (qsizetype index = victimScan - 1; index >= 0; --index) {
            if (!m_entries.at(index).descriptor.pinned) {
                victim = index;
                break;
            }
        }
        if (victim < 0) {
            outcome.error = ClipboardError::CapacityRefused;
            return outcome;
        }
        bytesAfter -= totalFormatBytes(m_entries.at(victim).value.formats);
        entriesAfter -= 1;
        victims.append(victim);
        victimScan = victim;
    }
    // Victims are strictly decreasing indices, so removing in this order
    // keeps the remaining indices valid.
    for (const qsizetype victim : victims) {
        m_entries.removeAt(victim);
    }
    m_totalPayloadBytes = bytesAfter;

    Entry entry;
    entry.value.formats = validation.canonicalFormats;
    entry.descriptor = makeDescriptor(entry.value, sourceLabel, tick);
    entry.descriptor.id = EntryId { m_generation, m_nextSerial };
    if (m_nextSerial == std::numeric_limits<quint32>::max()) {
        // The final serial of this generation was just consumed; latch
        // exhaustion instead of wrapping to a zero or duplicate identity.
        m_serialExhausted = true;
    } else {
        m_nextSerial += 1;
    }
    m_totalPayloadBytes += incomingBytes;
    m_entries.prepend(entry);
    bumpRevision();
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
    bumpRevision();
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
    bumpRevision();
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
    bumpRevision();
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
    bumpRevision();
    return outcome;
}

} // namespace QindaQt::Services::ClipboardModel
