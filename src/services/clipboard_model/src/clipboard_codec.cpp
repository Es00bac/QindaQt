// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_codec.h>

#include <qindaqt/services/clipboard_model/clipboard_media.h>

#include <QtCore/QSet>

#include "clipboard_codec_p.h"

namespace QindaQt::Services::ClipboardModel {

namespace {
constexpr char kValueMagic[] = "QCBV";
constexpr quint8 kValueVersion = 1;

// AGENT-CONTRACT: encode and decode enforce exactly the same value rules —
// count ceiling, canonical media, duplicate rejection, non-empty payload,
// per-item and aggregate size ceilings — in the same error vocabulary. Any
// rule added to one pass must be added to the other; an accepted encoding
// that its own decoder refuses would break the canonical round-trip claim.
// Both passes measure declared sizes before any payload byte is copied or
// appended, so refusal is allocation-free for payloads.
} // namespace

EncodedValue encodeValue(const ClipboardValue &value)
{
    EncodedValue result;
    // Measure pass: enforce every decoder rule before writing a byte.
    if (value.formats.isEmpty()) {
        result.error = ClipboardError::EmptyValue;
        return result;
    }
    if (value.formats.size() > kMaxFormatsPerItem) {
        result.error = ClipboardError::TooManyFormats;
        return result;
    }
    QSet<QString> seenMedia;
    qint64 totalBytes = 0;
    bool hasPayload = false;
    for (const ClipboardFormat &format : value.formats) {
        if (!isCanonicalMediaType(format.mediaType, kMaxMediaTypeLength)) {
            result.error = ClipboardError::MediaTypeRejected;
            return result;
        }
        if (seenMedia.contains(format.mediaType)) {
            result.error = ClipboardError::DuplicateFormat;
            return result;
        }
        seenMedia.insert(format.mediaType);
        const qint64 payloadBytes = format.payload.size();
        if (payloadBytes > kMaxItemPayloadBytes) {
            result.error = ClipboardError::OversizedValue;
            return result;
        }
        totalBytes += payloadBytes;
        if (payloadBytes > 0) {
            hasPayload = true;
        }
    }
    if (!hasPayload) {
        result.error = ClipboardError::EmptyValue;
        return result;
    }
    if (totalBytes > kMaxItemPayloadBytes) {
        result.error = ClipboardError::OversizedValue;
        return result;
    }

    // Write pass: all rules passed, framing is now bounded by construction.
    CodecDetail::ByteWriter writer;
    writer.raw(QByteArray(kValueMagic, 4));
    writer.u8(kValueVersion);
    writer.u16(static_cast<quint16>(value.formats.size()));
    for (const ClipboardFormat &format : value.formats) {
        writer.lengthPrefixedUtf8(format.mediaType);
        writer.u32(static_cast<quint32>(format.payload.size()));
        writer.raw(format.payload);
    }
    result.bytes = writer.buffer();
    return result;
}

DecodedValue decodeValue(const QByteArray &encoded)
{
    DecodedValue result;
    CodecDetail::ByteReader reader(encoded);
    if (!reader.readMagic(kValueMagic)) {
        result.error = ClipboardError::MalformedData;
        return result;
    }
    if (reader.u8() != kValueVersion) {
        result.error = ClipboardError::UnsupportedVersion;
        return result;
    }
    const quint16 formatCount = reader.u16();
    if (formatCount == 0 || formatCount > kMaxFormatsPerItem) {
        result.error = ClipboardError::TooManyFormats;
        return result;
    }
    qint64 totalBytes = 0;
    bool hasPayload = false;
    for (quint16 index = 0; index < formatCount; ++index) {
        const quint16 mediaLength = reader.u16();
        if (mediaLength == 0 || mediaLength > kMaxMediaTypeLength) {
            result.error = ClipboardError::MediaTypeRejected;
            return result;
        }
        const QByteArray mediaBytes = reader.sized(mediaLength);
        if (!reader.ok()) {
            result.error = ClipboardError::MalformedData;
            return result;
        }
        const QString mediaType = QString::fromUtf8(mediaBytes);
        if (!isCanonicalMediaType(mediaType, kMaxMediaTypeLength)) {
            result.error = ClipboardError::MediaTypeRejected;
            return result;
        }
        for (const ClipboardFormat &existing : result.value.formats) {
            if (existing.mediaType == mediaType) {
                result.error = ClipboardError::DuplicateFormat;
                return result;
            }
        }
        const quint32 payloadLength = reader.u32();
        if (payloadLength > static_cast<quint32>(kMaxItemPayloadBytes)) {
            result.error = ClipboardError::OversizedValue;
            return result;
        }
        const QByteArray payload = reader.sized(static_cast<qsizetype>(payloadLength));
        if (!reader.ok()) {
            result.error = ClipboardError::MalformedData;
            return result;
        }
        totalBytes += static_cast<qint64>(payloadLength);
        if (payloadLength > 0) {
            hasPayload = true;
        }
        result.value.formats.append(ClipboardFormat { mediaType, payload });
    }
    if (!hasPayload) {
        result.error = ClipboardError::EmptyValue;
        return result;
    }
    if (totalBytes > kMaxItemPayloadBytes) {
        result.error = ClipboardError::OversizedValue;
        return result;
    }
    if (!reader.atEnd()) {
        // AGENT-GUARD: trailing bytes mean the peer encoded something this
        // version cannot understand; accepting the prefix would make the
        // canonical form ambiguous for future versions.
        result.error = ClipboardError::MalformedData;
        return result;
    }
    return result;
}

} // namespace QindaQt::Services::ClipboardModel
