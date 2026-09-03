// SPDX-License-Identifier: LGPL-3.0-or-later

#include "icc_text_metadata_p.h"

#include <QtCore/QtEndian>
#include <QtCore/QtGlobal>

namespace QindaQt::DisplayColor
{
namespace
{

constexpr quint32 DescTagSignature = 0x64657363; // 'desc'
constexpr quint32 MlucTagSignature = 0x6d6c7563; // 'mluc'
constexpr quint32 TextDescriptionSignature = 0x64657363; // 'desc' type
constexpr quint32 MultiLocalizedSignature = 0x6d6c7563; // 'mluc' type
constexpr quint32 TagTableEntrySize = 12;

bool readFourCc(const QByteArray &data, quint32 &signature)
{
    if (data.size() < 4) {
        return false;
    }
    signature = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData()));
    return true;
}

// 'desc' (textDescriptionType): 'desc', reserved u32, ASCII count u32
// (including the terminator), ASCII bytes. The count is capped by the
// description budget before any use.
QString parseTextDescription(const QByteArray &payload)
{
    quint32 type = 0;
    if (!readFourCc(payload, type) || type != TextDescriptionSignature || payload.size() < 12) {
        return {};
    }
    const quint32 asciiCount =
        qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(payload.constData()) + 8);
    if (asciiCount < 2 || asciiCount > static_cast<quint32>(payload.size() - 12)) {
        // A count of 0/1 cannot carry a non-empty terminated string, and a
        // count beyond the supplied payload is hostile truncation.
        return {};
    }
    QByteArray ascii = payload.mid(12, static_cast<qsizetype>(asciiCount));
    if (ascii.endsWith('\0')) {
        ascii.chop(1);
    }
    if (ascii.isEmpty()) {
        return {};
    }
    const QString decoded = QString::fromUtf8(ascii);
    return decoded;
}

// 'mluc' (multiLocalizedUnicode): 'mluc', reserved u32, record count u32,
// record size u32 (12), then {language u16, country u16, length u32,
// offset u32} records holding UTF-16BE text. The first record wins; the
// choice is deterministic and the ICC default locale record is conventionally
// first.
QString parseMultiLocalizedDescription(const QByteArray &payload, const IccRegionReader &readRegion,
                                       quint32 tagLength, quint32 descriptionTagBytesCap)
{
    quint32 type = 0;
    // 16 header bytes plus the full 12-byte first record must be inside the
    // supplied payload before any record field is read.
    if (!readFourCc(payload, type) || type != MultiLocalizedSignature || payload.size() < 28) {
        return {};
    }
    const uchar *base = reinterpret_cast<const uchar *>(payload.constData());
    const quint32 recordCount = qFromBigEndian<quint32>(base + 8);
    const quint32 recordSize = qFromBigEndian<quint32>(base + 12);
    if (recordCount == 0 || recordCount > 64 || recordSize != 12) {
        return {};
    }
    // Record layout: language u16, country u16, byte length u32, payload
    // offset u32 (relative to the tag payload start).
    const quint32 recordLength = qFromBigEndian<quint32>(base + 16 + 4);
    const quint32 recordOffset = qFromBigEndian<quint32>(base + 16 + 8);
    if (recordLength == 0 || recordLength > descriptionTagBytesCap) {
        return {};
    }
    if (recordOffset > tagLength || recordLength > tagLength - recordOffset) {
        return {};
    }
    // The tag payload itself was already fetched bounded; records outside it
    // (defective or hostile tags) resolve through the same bounded reader so
    // both layouts are served without unbounded reads.
    const QByteArray text = (recordOffset + recordLength <= static_cast<quint32>(payload.size()))
                                ? payload.mid(static_cast<qsizetype>(recordOffset),
                                              static_cast<qsizetype>(recordLength))
                                : readRegion(recordOffset, recordLength);
    if (text.isEmpty() || (text.size() % 2) != 0) {
        return {};
    }
    QString decoded(static_cast<qsizetype>(text.size() / 2), QChar(u'\0'));
    for (qsizetype i = 0; i < decoded.size(); ++i) {
        const uchar high = static_cast<uchar>(text.at(i * 2));
        const uchar low = static_cast<uchar>(text.at(i * 2 + 1));
        decoded[i] = QChar(static_cast<char16_t>((high << 8) | low));
    }
    return decoded;
}

} // namespace

IccTextMetadata extractIccDescription(const IccRegionReader &readRegion,
                                      quint32 tagTableEntries,
                                      quint32 tagTableEntriesCap,
                                      quint32 descriptionTagBytesCap)
{
    IccTextMetadata metadata;
    if (!readRegion || tagTableEntries == 0) {
        return metadata;
    }
    metadata.tagTableTruncated = tagTableEntries > tagTableEntriesCap;
    const quint32 boundedEntries = qMin(tagTableEntries, tagTableEntriesCap);

    // The tag count occupies the four bytes after the header; entries begin
    // immediately after it (ICC v2+ tag table layout).
    const QByteArray tagTable =
        readRegion(IccHeaderSizeBytes + 4, boundedEntries * TagTableEntrySize);
    for (quint32 i = 0; i < boundedEntries; ++i) {
        const uchar *entry = reinterpret_cast<const uchar *>(tagTable.constData()) +
                             static_cast<qsizetype>(i) * TagTableEntrySize;
        if (static_cast<quint32>(tagTable.size()) < (i + 1) * TagTableEntrySize) {
            break;
        }
        const quint32 signature = qFromBigEndian<quint32>(entry);
        if (signature != DescTagSignature && signature != MlucTagSignature) {
            continue;
        }
        const quint32 tagOffset = qFromBigEndian<quint32>(entry + 4);
        const quint32 tagLength = qFromBigEndian<quint32>(entry + 8);
        if (tagLength > descriptionTagBytesCap) {
            metadata.descriptionTagOversized = true;
            continue;
        }
        if (tagLength == 0) {
            continue;
        }
        const QByteArray payload = readRegion(tagOffset, tagLength);
        if (payload.isEmpty()) {
            continue;
        }
        QString decoded;
        if (signature == DescTagSignature) {
            decoded = parseTextDescription(payload);
        } else {
            decoded = parseMultiLocalizedDescription(payload, readRegion, tagLength,
                                                     descriptionTagBytesCap);
        }
        if (!decoded.isEmpty()) {
            metadata.description = decoded;
            metadata.descriptionValid = true;
            return metadata;
        }
    }
    return metadata;
}

QString chooseDiscoveredDisplayName(const IccTextMetadata &metadata, const QString &fallbackName)
{
    if (metadata.descriptionValid) {
        const QStringList lines = metadata.description.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) {
                continue;
            }
            if (trimmed.size() <= MaxDisplayNameLength) {
                return trimmed;
            }
            // The first non-blank line is the deterministic candidate; an
            // overlong one falls back rather than truncating published text.
            break;
        }
    }
    return fallbackName;
}

QString sanitizeProfileIdFromFileName(const QString &fileName)
{
    QString sanitized;
    sanitized.reserve(fileName.size());
    for (const QChar ch : fileName) {
        const ushort u = ch.unicode();
        const bool asciiAlnum = (u >= u'a' && u <= u'z') || (u >= u'A' && u <= u'Z') ||
                                (u >= u'0' && u <= u'9');
        if (asciiAlnum || u == u'.' || u == u':' || u == u'-' || u == u'_') {
            sanitized.append(ch);
        }
    }
    if (sanitized.isEmpty() || sanitized.size() > MaxIdentifierLength) {
        return {};
    }
    return sanitized;
}

QString iccStatusDetail(ProfileValidationStatus status)
{
    switch (status) {
    case ProfileValidationStatus::Valid:
        return QStringLiteral("Valid");
    case ProfileValidationStatus::EmptyData:
        return QStringLiteral("EmptyData");
    case ProfileValidationStatus::HeaderTooSmall:
        return QStringLiteral("HeaderTooSmall");
    case ProfileValidationStatus::InvalidMagic:
        return QStringLiteral("InvalidMagic");
    case ProfileValidationStatus::InvalidDeclaredSize:
        return QStringLiteral("InvalidDeclaredSize");
    case ProfileValidationStatus::InvalidSize:
        return QStringLiteral("InvalidSize");
    case ProfileValidationStatus::UnsupportedColorSpace:
        return QStringLiteral("UnsupportedColorSpace");
    case ProfileValidationStatus::UnsupportedConnectionSpace:
        return QStringLiteral("UnsupportedConnectionSpace");
    case ProfileValidationStatus::UnsupportedProfileClass:
        return QStringLiteral("UnsupportedProfileClass");
    case ProfileValidationStatus::ChecksumMismatch:
        return QStringLiteral("ChecksumMismatch");
    case ProfileValidationStatus::Oversized:
        return QStringLiteral("Oversized");
    case ProfileValidationStatus::MalformedMetadata:
        return QStringLiteral("MalformedMetadata");
    case ProfileValidationStatus::InvalidVersion:
        return QStringLiteral("InvalidVersion");
    }
    return QStringLiteral("Unknown");
}

QString iccFileStem(const QString &fileName)
{
    const qsizetype dot = fileName.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0) {
        return fileName;
    }
    return fileName.left(dot);
}

IccRegionReader memoryRegionReader(const QByteArray &content)
{
    return [&content](quint32 offset, quint32 length) -> QByteArray {
        if (length == 0 || offset > static_cast<quint64>(content.size()) ||
            length > static_cast<quint64>(content.size()) - offset) {
            return {};
        }
        return content.mid(static_cast<qsizetype>(offset), static_cast<qsizetype>(length));
    };
}

IccProfileDescriptor assembleDescriptor(DiscoveryOrigin origin, const QString &fileName,
                                        const QByteArray &headerBytes, quint32 fileSize,
                                        const IccTextMetadata &metadata,
                                        const QByteArray &lineageFingerprint)
{
    IccProfileDescriptor descriptor;
    descriptor.profileId = sanitizeProfileIdFromFileName(iccFileStem(fileName));
    descriptor.displayName = chooseDiscoveredDisplayName(metadata, descriptor.profileId);
    descriptor.description = metadata.descriptionValid ? metadata.description : QString();
    descriptor.fileName = fileName;
    switch (origin) {
    case DiscoveryOrigin::BuiltIn:
        descriptor.origin = ProfileOrigin::BuiltIn;
        break;
    case DiscoveryOrigin::System:
        descriptor.origin = ProfileOrigin::System;
        break;
    case DiscoveryOrigin::UserImported:
        descriptor.origin = ProfileOrigin::UserImported;
        break;
    }
    descriptor.gamut = ColorSpaceGamut::Custom;
    descriptor.transferFunction = TransferFunction::Gamma22;
    descriptor.rawHeader = headerBytes;
    descriptor.checksumSha256 = lineageFingerprint;
    descriptor.byteSize = fileSize;
    descriptor.wireValid = true;
    return descriptor;
}

bool destinationNameIsSafe(const QString &fileName)
{
    if (fileName.isEmpty() || fileName.size() > MaxFilenameLength) {
        return false;
    }
    // AGENT-GUARD: Discovery lists with QDir::Files and never sees dot
    // names, so a dot-prefixed import would store a profile that can never
    // re-enter the catalog and would break the import/discovery round trip;
    // reject it here instead.
    if (fileName.startsWith(QLatin1Char('.'))) {
        return false;
    }
    if (fileName.contains(u'/') || fileName.contains(u'\\') ||
        fileName.contains(QStringLiteral(".."))) {
        return false;
    }
    for (const QChar ch : fileName) {
        if (ch.isSpace() || ch.unicode() < 0x20 || ch.unicode() == 0x7F) {
            return false;
        }
    }
    return true;
}

} // namespace QindaQt::DisplayColor
