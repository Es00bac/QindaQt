// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_types.h>

#include <QtCore/QByteArray>
#include <QtCore/QList>

namespace QindaQt::Services::ClipboardModel {

struct EncodedDescriptor {
    ClipboardError error = ClipboardError::None;
    QByteArray bytes;

    [[nodiscard]] bool accepted() const noexcept { return error == ClipboardError::None; }
};

struct DecodedDescriptor {
    ClipboardError error = ClipboardError::None;
    ClipboardEntryDescriptor descriptor;

    [[nodiscard]] bool accepted() const noexcept { return error == ClipboardError::None; }
};

struct EncodedDescriptorList {
    ClipboardError error = ClipboardError::None;
    QByteArray bytes;

    [[nodiscard]] bool accepted() const noexcept { return error == ClipboardError::None; }
};

struct DecodedDescriptorList {
    ClipboardError error = ClipboardError::None;
    QList<ClipboardEntryDescriptor> descriptors;

    [[nodiscard]] bool accepted() const noexcept { return error == ClipboardError::None; }
};

// Canonical metadata-only descriptor codecs ("QCBD" entry, "QCDL" list,
// format 1). Descriptors carry identity, ticks, pins, source label, preview,
// format names with byte counts, and the fingerprint — never payload bytes.
// Encode and decode share one validation floor: valid identity, nonempty
// bounded canonical format list with unique names, non-negative per-format
// and aggregate claimed bytes, sanitized label/preview metadata, consistent
// truncation flag, and exact fingerprint width. Decode applies the shared
// hostile-input framing rules plus the same floor, so an accepted encoding
// always decodes and a refused entry or list exposes no partial descriptor
// content. The future Clipboard1 snapshot transport is expected to reuse the
// list form so presentation never needs its own serialization; a descriptor
// list reports TooManyEntries (not TooManyFormats) when its entry count
// exceeds the protocol bound.
[[nodiscard]] EncodedDescriptor encodeDescriptor(const ClipboardEntryDescriptor &descriptor);
[[nodiscard]] DecodedDescriptor decodeDescriptor(const QByteArray &encoded);
[[nodiscard]] EncodedDescriptorList encodeDescriptorList(
    const QList<ClipboardEntryDescriptor> &descriptors);
[[nodiscard]] DecodedDescriptorList decodeDescriptorList(const QByteArray &encoded);

} // namespace QindaQt::Services::ClipboardModel
