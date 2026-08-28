// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_types.h>

#include <QtCore/QtGlobal>

namespace QindaQt::Services::ClipboardModel {

bool isValidLimits(const HistoryLimits &limits) noexcept
{
    if (limits.maxEntries < 1 || limits.maxEntries > kMaxEntries) {
        return false;
    }
    if (limits.maxPinnedEntries < 0
        || limits.maxPinnedEntries > qMin(kMaxPinnedEntries, limits.maxEntries)) {
        return false;
    }
    if (limits.maxFormatsPerItem < 1 || limits.maxFormatsPerItem > kMaxFormatsPerItem) {
        return false;
    }
    if (limits.maxMediaTypeLength < 4 || limits.maxMediaTypeLength > kMaxMediaTypeLength) {
        return false;
    }
    if (limits.maxSourceLabelCodeUnits < 0
        || limits.maxSourceLabelCodeUnits > kMaxSourceLabelCodeUnits) {
        return false;
    }
    if (limits.maxPreviewCodeUnits < 1 || limits.maxPreviewCodeUnits > kMaxPreviewCodeUnits) {
        return false;
    }
    if (limits.maxItemPayloadBytes < 1 || limits.maxItemPayloadBytes > kMaxItemPayloadBytes) {
        return false;
    }
    if (limits.maxTotalPayloadBytes < limits.maxItemPayloadBytes
        || limits.maxTotalPayloadBytes > kMaxTotalPayloadBytes) {
        return false;
    }
    return true;
}

// AGENT-GUARD: construction is the only place widened limits could enter the
// model. Clamping here (rather than asserting) is what makes the protocol
// ceilings hold in Release builds; the model must never consult an unclamped
// field afterwards.
HistoryLimits sanitizeLimits(const HistoryLimits &limits) noexcept
{
    HistoryLimits sanitized;
    sanitized.maxEntries = qBound(1, limits.maxEntries, kMaxEntries);
    sanitized.maxPinnedEntries = qBound(0, limits.maxPinnedEntries,
                                        qMin(kMaxPinnedEntries, sanitized.maxEntries));
    sanitized.maxFormatsPerItem = qBound(1, limits.maxFormatsPerItem, kMaxFormatsPerItem);
    sanitized.maxMediaTypeLength = qBound(4, limits.maxMediaTypeLength, kMaxMediaTypeLength);
    sanitized.maxSourceLabelCodeUnits =
        qBound(0, limits.maxSourceLabelCodeUnits, kMaxSourceLabelCodeUnits);
    sanitized.maxPreviewCodeUnits = qBound(1, limits.maxPreviewCodeUnits, kMaxPreviewCodeUnits);
    sanitized.maxItemPayloadBytes =
        qBound(qint64 { 1 }, limits.maxItemPayloadBytes, kMaxItemPayloadBytes);
    sanitized.maxTotalPayloadBytes = qBound(sanitized.maxItemPayloadBytes,
                                            limits.maxTotalPayloadBytes,
                                            kMaxTotalPayloadBytes);
    return sanitized;
}

HistoryCounters sanitizeCounters(const HistoryCounters &counters) noexcept
{
    HistoryCounters sanitized;
    // Zero is not a valid lineage value (EntryId declares it invalid); wrap
    // attempts must never be laundered through the seam either, so out-of-
    // range starts clamp rather than alias the exhausted state.
    sanitized.generation = counters.generation == 0 ? quint32 { 1 } : counters.generation;
    sanitized.nextSerial = counters.nextSerial == 0 ? quint32 { 1 } : counters.nextSerial;
    sanitized.revision = counters.revision;
    return sanitized;
}

} // namespace QindaQt::Services::ClipboardModel
