// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_protocol/envelope.h>

#include <QtCore/QByteArray>

namespace QindaQt::Link {

enum class DecodeStatus { Frame, NeedsMore, BadLength, BadPayload, BadEnvelope };

struct DecodeResult {
    DecodeStatus status = DecodeStatus::NeedsMore;
    Envelope envelope;  // valid only when status == Frame
};

// Incremental decoder for the 4-byte big-endian length framing. Not thread
// safe; one instance per connection.
//
// AGENT-CONTRACT: any status other than Frame/NeedsMore is terminal. The
// caller closes the connection without replying and discards the decoder;
// further calls keep returning the same error.
class FrameDecoder {
public:
    void append(const QByteArray &bytes);
    // Returns the next complete frame, NeedsMore, or a terminal error.
    [[nodiscard]] DecodeResult next();
    [[nodiscard]] qsizetype bufferedBytes() const { return m_buffer.size() - m_offset; }

private:
    QByteArray m_buffer;
    qsizetype m_offset = 0;  // consumed prefix; compacted in append()
    DecodeStatus m_failed = DecodeStatus::NeedsMore;
};

// Decodes one payload (no length prefix). Exposed for tests and for callers
// that already hold a framed payload.
[[nodiscard]] DecodeResult decodePayload(const QByteArray &payload);

// Compact JSON with the 4-byte length prefix. Returns an empty array when the
// encoded payload would exceed kMaxPayloadBytes.
[[nodiscard]] QByteArray encodeFrame(const Envelope &envelope);

} // namespace QindaQt::Link
