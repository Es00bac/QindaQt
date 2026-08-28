// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_types.h>

namespace QindaQt::Services::ClipboardModel::HistoryDetail {

// Sum of payload byte counts across a format list; the per-item and
// history-total capacity accounting uses this everywhere.
[[nodiscard]] qint64 totalFormatBytes(const QList<ClipboardFormat> &formats) noexcept;

// AGENT-GUARD: dedup equality and descriptor equality both rely on this
// fingerprint covering media types, payload lengths, and payload bytes in
// stored order. Changing the composition without bumping the codec/descriptor
// documentation silently makes dedup alias distinct items.
[[nodiscard]] QByteArray entryFingerprint(const QList<ClipboardFormat> &canonicalFormats);

struct DerivedPreview {
    QString preview;
    bool truncated = false;

    friend bool operator==(const DerivedPreview &, const DerivedPreview &) = default;
};

// Derives the bounded presentation preview from the canonical text/plain
// payload when present: control/format characters become spaces and the
// result is clamped to maxCodeUnits with truncated set.
[[nodiscard]] DerivedPreview derivePreview(const QList<ClipboardFormat> &canonicalFormats,
                                           int maxCodeUnits);

} // namespace QindaQt::Services::ClipboardModel::HistoryDetail
