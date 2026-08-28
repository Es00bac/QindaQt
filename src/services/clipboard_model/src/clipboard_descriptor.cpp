// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>

#include <limits>

#include <qindaqt/services/clipboard_model/clipboard_media.h>

#include <QtCore/QSet>

#include "clipboard_codec_p.h"

namespace QindaQt::Services::ClipboardModel {

namespace {

constexpr char kDescriptorMagic[] = "QCBD";
constexpr char kDescriptorListMagic[] = "QCDL";
constexpr quint8 kDescriptorVersion = 1;
constexpr quint8 kFlagPinned = 0x01;
constexpr quint8 kFlagPreviewTruncated = 0x02;
constexpr qsizetype kFingerprintBytes = 32;

// AGENT-CONTRACT: this is the single descriptor floor, shared by encode and
// decode. It enforces the same hostile-input rules the wiki promises: valid
// generation-tagged identity, nonempty bounded canonical format list with
// unique names, non-negative per-format and aggregate claimed bytes, exactly
// one fingerprint width, and producer metadata that already satisfies the
// model's sanitization contract (labels and previews carry no control or
// format characters, previews obey the truncation flag). Encode refuses
// before writing; decode refuses after parsing, before returning.
ClipboardError validateDescriptor(const ClipboardEntryDescriptor &descriptor)
{
    if (!descriptor.id.isValid()) {
        return ClipboardError::MalformedData;
    }
    if (descriptor.sourceLabel.size() > kMaxSourceLabelCodeUnits
        || descriptor.sourceLabel
            != sanitizeSourceLabel(descriptor.sourceLabel, kMaxSourceLabelCodeUnits)) {
        return ClipboardError::MalformedData;
    }
    if (descriptor.preview.size() > kMaxPreviewCodeUnits) {
        return ClipboardError::MalformedData;
    }
    for (const QChar ch : descriptor.preview) {
        const QChar::Category category = ch.category();
        if (category == QChar::Other_Control || category == QChar::Other_Format) {
            return ClipboardError::MalformedData;
        }
    }
    // The truncation flag's protocol-checkable content: only the producing
    // model knows its own clamp width, so an exact-width match is NOT wire-
    // enforceable for instances with narrowed limits. A flagged empty
    // preview, however, is impossible under any width and always means a
    // forged or corrupted form.
    if (descriptor.previewTruncated && descriptor.preview.isEmpty()) {
        return ClipboardError::MalformedData;
    }
    if (descriptor.fingerprint.size() != kFingerprintBytes) {
        return ClipboardError::MalformedData;
    }
    if (descriptor.formats.isEmpty()) {
        return ClipboardError::EmptyValue;
    }
    if (descriptor.formats.size() > kMaxFormatsPerItem) {
        return ClipboardError::TooManyFormats;
    }
    QSet<QString> seenMedia;
    qint64 aggregateBytes = 0;
    for (const FormatInfo &format : descriptor.formats) {
        if (!isCanonicalMediaType(format.mediaType, kMaxMediaTypeLength)) {
            return ClipboardError::MediaTypeRejected;
        }
        if (seenMedia.contains(format.mediaType)) {
            return ClipboardError::DuplicateFormat;
        }
        seenMedia.insert(format.mediaType);
        if (format.payloadBytes < 0) {
            return ClipboardError::MalformedData;
        }
        if (format.payloadBytes > kMaxItemPayloadBytes) {
            return ClipboardError::OversizedValue;
        }
        aggregateBytes += format.payloadBytes;
        if (aggregateBytes > kMaxItemPayloadBytes) {
            return ClipboardError::OversizedValue;
        }
    }
    return ClipboardError::None;
}

} // namespace

EncodedDescriptor encodeDescriptor(const ClipboardEntryDescriptor &descriptor)
{
    EncodedDescriptor result;
    const ClipboardError shapeError = validateDescriptor(descriptor);
    if (shapeError != ClipboardError::None) {
        result.error = shapeError;
        return result;
    }
    quint8 flags = 0;
    if (descriptor.pinned) {
        flags |= kFlagPinned;
    }
    if (descriptor.previewTruncated) {
        flags |= kFlagPreviewTruncated;
    }
    CodecDetail::ByteWriter writer;
    writer.raw(QByteArray(kDescriptorMagic, 4));
    writer.u8(kDescriptorVersion);
    writer.u32(descriptor.id.generation);
    writer.u32(descriptor.id.serial);
    writer.u64(descriptor.admittedTick);
    writer.u64(descriptor.lastUsedTick);
    writer.u8(flags);
    writer.lengthPrefixedUtf8(descriptor.sourceLabel);
    writer.lengthPrefixedUtf8(descriptor.preview);
    writer.u16(static_cast<quint16>(descriptor.formats.size()));
    for (const FormatInfo &format : descriptor.formats) {
        writer.lengthPrefixedUtf8(format.mediaType);
        writer.u32(static_cast<quint32>(format.payloadBytes));
    }
    writer.raw(descriptor.fingerprint);
    result.bytes = writer.buffer();
    return result;
}

DecodedDescriptor decodeDescriptor(const QByteArray &encoded)
{
    DecodedDescriptor result;
    CodecDetail::ByteReader reader(encoded);
    if (!reader.readMagic(kDescriptorMagic)) {
        result.error = ClipboardError::MalformedData;
        return result;
    }
    if (reader.u8() != kDescriptorVersion) {
        result.error = ClipboardError::UnsupportedVersion;
        return result;
    }
    ClipboardEntryDescriptor descriptor;
    descriptor.id.generation = reader.u32();
    descriptor.id.serial = reader.u32();
    descriptor.admittedTick = reader.u64();
    descriptor.lastUsedTick = reader.u64();
    if (!reader.ok()) {
        result.error = ClipboardError::MalformedData;
        return result;
    }
    const quint8 flags = reader.u8();
    if ((flags & ~quint8(kFlagPinned | kFlagPreviewTruncated)) != 0) {
        result.error = ClipboardError::MalformedData;
        return result;
    }
    descriptor.pinned = (flags & kFlagPinned) != 0;
    descriptor.previewTruncated = (flags & kFlagPreviewTruncated) != 0;
    descriptor.sourceLabel = QString::fromUtf8(reader.sized(reader.u16()));
    descriptor.preview = QString::fromUtf8(reader.sized(reader.u16()));
    if (!reader.ok()) {
        result.error = ClipboardError::MalformedData;
        return result;
    }
    const quint16 formatCount = reader.u16();
    if (formatCount == 0 || formatCount > kMaxFormatsPerItem) {
        result.error = formatCount == 0 ? ClipboardError::EmptyValue
                                        : ClipboardError::TooManyFormats;
        return result;
    }
    for (quint16 index = 0; index < formatCount; ++index) {
        const quint16 mediaLength = reader.u16();
        if (mediaLength == 0 || mediaLength > kMaxMediaTypeLength) {
            result.error = ClipboardError::MediaTypeRejected;
            return result;
        }
        const QByteArray mediaBytes = reader.sized(mediaLength);
        const QString mediaType = QString::fromUtf8(mediaBytes);
        const quint32 payloadBytes = reader.u32();
        if (!reader.ok()) {
            result.error = ClipboardError::MalformedData;
            return result;
        }
        if (!isCanonicalMediaType(mediaType, kMaxMediaTypeLength)) {
            result.error = ClipboardError::MediaTypeRejected;
            return result;
        }
        if (payloadBytes > static_cast<quint32>(kMaxItemPayloadBytes)) {
            result.error = ClipboardError::OversizedValue;
            return result;
        }
        descriptor.formats.append(FormatInfo { mediaType, static_cast<qint64>(payloadBytes) });
    }
    descriptor.fingerprint = reader.sized(kFingerprintBytes);
    if (!reader.ok() || !reader.atEnd()) {
        result.error = ClipboardError::MalformedData;
        return result;
    }
    // Shared floor: the parsed form must satisfy the identical validation
    // the encoder used, including aggregate and sanitization rules.
    result.error = validateDescriptor(descriptor);
    if (result.error != ClipboardError::None) {
        return result;
    }
    result.descriptor = descriptor;
    return result;
}

EncodedDescriptorList encodeDescriptorList(const QList<ClipboardEntryDescriptor> &descriptors)
{
    EncodedDescriptorList result;
    if (descriptors.size() > kMaxEntries) {
        result.error = ClipboardError::TooManyEntries;
        return result;
    }
    CodecDetail::ByteWriter listWriter;
    listWriter.raw(QByteArray(kDescriptorListMagic, 4));
    listWriter.u8(kDescriptorVersion);
    listWriter.u16(static_cast<quint16>(descriptors.size()));
    for (const ClipboardEntryDescriptor &descriptor : descriptors) {
        const EncodedDescriptor encoded = encodeDescriptor(descriptor);
        if (!encoded.accepted()) {
            result.error = encoded.error;
            return result;
        }
        if (encoded.bytes.size() > std::numeric_limits<quint16>::max()) {
            result.error = ClipboardError::OversizedValue;
            return result;
        }
        listWriter.u16(static_cast<quint16>(encoded.bytes.size()));
        listWriter.raw(encoded.bytes);
    }
    result.bytes = listWriter.buffer();
    return result;
}

DecodedDescriptorList decodeDescriptorList(const QByteArray &encoded)
{
    DecodedDescriptorList result;
    CodecDetail::ByteReader reader(encoded);
    if (!reader.readMagic(kDescriptorListMagic)) {
        result.error = ClipboardError::MalformedData;
        return result;
    }
    if (reader.u8() != kDescriptorVersion) {
        result.error = ClipboardError::UnsupportedVersion;
        return result;
    }
    const quint16 count = reader.u16();
    if (count > kMaxEntries) {
        result.error = ClipboardError::TooManyEntries;
        return result;
    }
    for (quint16 index = 0; index < count; ++index) {
        const quint16 blobLength = reader.u16();
        const QByteArray blob = reader.sized(blobLength);
        if (!reader.ok()) {
            result.error = ClipboardError::MalformedData;
            return result;
        }
        const DecodedDescriptor decoded = decodeDescriptor(blob);
        if (!decoded.accepted()) {
            result.error = decoded.error;
            return result;
        }
        result.descriptors.append(decoded.descriptor);
    }
    if (!reader.atEnd()) {
        result.error = ClipboardError::MalformedData;
        return result;
    }
    return result;
}

} // namespace QindaQt::Services::ClipboardModel
