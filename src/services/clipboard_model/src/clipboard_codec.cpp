// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_codec.h>

#include <qindaqt/services/clipboard_model/clipboard_media.h>

#include <QtCore/QSet>

#include <utility>

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
// appended, so refusal is allocation-free for payloads. Decode stages all
// content privately and publishes only after the complete form succeeds;
// every refusal therefore exposes an empty DecodedValue.
ClipboardError scanEncodedValue(const QByteArray &encoded)
{
    CodecDetail::ByteReader reader(encoded);
    if (!reader.readMagic(kValueMagic)) {
        return ClipboardError::MalformedData;
    }
    if (reader.u8() != kValueVersion) {
        return ClipboardError::UnsupportedVersion;
    }
    const quint16 formatCount = reader.u16();
    if (!reader.ok()) {
        return ClipboardError::MalformedData;
    }
    if (formatCount == 0) {
        return ClipboardError::EmptyValue;
    }
    if (formatCount > kMaxFormatsPerItem) {
        return ClipboardError::TooManyFormats;
    }

    QSet<QString> seenMedia;
    qint64 totalBytes = 0;
    bool hasPayload = false;
    for (quint16 index = 0; index < formatCount; ++index) {
        const quint16 mediaLength = reader.u16();
        if (!reader.ok()) {
            return ClipboardError::MalformedData;
        }
        if (mediaLength == 0 || mediaLength > kMaxMediaTypeLength) {
            return ClipboardError::MediaTypeRejected;
        }
        const QByteArray mediaBytes = reader.sized(mediaLength);
        if (!reader.ok()) {
            return ClipboardError::MalformedData;
        }
        const QString mediaType = QString::fromUtf8(mediaBytes);
        if (!isCanonicalMediaType(mediaType, kMaxMediaTypeLength)) {
            return ClipboardError::MediaTypeRejected;
        }
        if (seenMedia.contains(mediaType)) {
            return ClipboardError::DuplicateFormat;
        }
        seenMedia.insert(mediaType);

        const quint32 payloadLength = reader.u32();
        if (!reader.ok()) {
            return ClipboardError::MalformedData;
        }
        if (payloadLength > static_cast<quint32>(kMaxItemPayloadBytes)
            || static_cast<qint64>(payloadLength) > kMaxItemPayloadBytes - totalBytes) {
            return ClipboardError::OversizedValue;
        }
        // AGENT-GUARD: scan the payload extent but never call sized() here.
        // This first pass must remain payload-allocation-free on every refusal.
        if (!reader.skip(static_cast<qsizetype>(payloadLength))) {
            return ClipboardError::MalformedData;
        }
        totalBytes += static_cast<qint64>(payloadLength);
        hasPayload = hasPayload || payloadLength > 0;
    }
    if (!hasPayload) {
        return ClipboardError::EmptyValue;
    }
    if (!reader.atEnd()) {
        return ClipboardError::MalformedData;
    }
    return ClipboardError::None;
}
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
    result.error = scanEncodedValue(encoded);
    if (result.error != ClipboardError::None) {
        return result;
    }

    // Materialization is a second pass over the already validated immutable
    // byte form. Keep the value private until every read succeeds so even a
    // future scanner/materializer drift cannot leak a partial rejected value.
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
    if (!reader.ok()) {
        result.error = ClipboardError::MalformedData;
        return result;
    }
    ClipboardValue stagedValue;
    stagedValue.formats.reserve(formatCount);
    for (quint16 index = 0; index < formatCount; ++index) {
        const quint16 mediaLength = reader.u16();
        const QByteArray mediaBytes = reader.sized(mediaLength);
        if (!reader.ok()) {
            result.error = ClipboardError::MalformedData;
            return result;
        }
        const QString mediaType = QString::fromUtf8(mediaBytes);
        const quint32 payloadLength = reader.u32();
        const QByteArray payload = reader.sized(static_cast<qsizetype>(payloadLength));
        if (!reader.ok()) {
            result.error = ClipboardError::MalformedData;
            return result;
        }
        stagedValue.formats.append(ClipboardFormat { mediaType, payload });
    }
    if (!reader.ok() || !reader.atEnd()) {
        // AGENT-GUARD: trailing bytes mean the peer encoded something this
        // version cannot understand; accepting the prefix would make the
        // canonical form ambiguous for future versions.
        result.error = ClipboardError::MalformedData;
        return result;
    }
    result.value = std::move(stagedValue);
    return result;
}

} // namespace QindaQt::Services::ClipboardModel
