// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_types.h>

#include <QtCore/QByteArray>

namespace QindaQt::Services::ClipboardModel {

struct EncodedValue {
    ClipboardError error = ClipboardError::None;
    QByteArray bytes;

    [[nodiscard]] bool accepted() const noexcept { return error == ClipboardError::None; }
};

struct DecodedValue {
    ClipboardError error = ClipboardError::None;
    ClipboardValue value;

    [[nodiscard]] bool accepted() const noexcept { return error == ClipboardError::None; }
};

// Canonical value codec ("QCBV", format 1). Encodes the canonical format
// list with media names and payloads; decodes under the fixed protocol
// bounds, rejecting unknown versions, truncation, declared sizes beyond the
// remaining buffer, trailing bytes, duplicate or non-canonical media names,
// and values with no formats or no payload bytes. The future Clipboard1
// transport may reuse this form only for bounded inline payloads; large
// transfers still move by FD and never through this codec.
[[nodiscard]] EncodedValue encodeValue(const ClipboardValue &value);
[[nodiscard]] DecodedValue decodeValue(const QByteArray &encoded);

} // namespace QindaQt::Services::ClipboardModel
