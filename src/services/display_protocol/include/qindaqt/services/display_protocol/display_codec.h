// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_types.h>

#include <QtCore/QByteArrayView>

namespace QindaQt::Display
{

enum class CodecError {
    None,
    InvalidValue,
    PayloadTooLarge,
    Truncated,
    InvalidMagic,
    UnsupportedCodecVersion,
};

struct EncodeResult {
    QByteArray payload;
    CodecError error = CodecError::None;
    QString reasonCode;

    [[nodiscard]] bool succeeded() const noexcept { return error == CodecError::None; }
};

struct DecodeResult {
    CodecError error = CodecError::None;
    QString reasonCode;

    [[nodiscard]] bool succeeded() const noexcept { return error == CodecError::None; }
};

// AGENT-CONTRACT: These pure functions borrow inputs for one call, return
// owned results, and are reentrant on any thread. Decoders consume hostile
// bytes into a temporary and replace destination only after complete semantic
// validation. Callers may therefore retain a previously published value
// verbatim on every typed failure. Byte-layout changes require a new accepted
// codec/protocol version.
[[nodiscard]] EncodeResult encodeCandidate(const Candidate &candidate);
[[nodiscard]] DecodeResult decodeCandidate(QByteArrayView payload, Candidate &destination);
[[nodiscard]] EncodeResult encodeSnapshot(const Snapshot &snapshot);
[[nodiscard]] DecodeResult decodeSnapshot(QByteArrayView payload, Snapshot &destination);

} // namespace QindaQt::Display
