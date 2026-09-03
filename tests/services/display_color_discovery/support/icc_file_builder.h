// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QtEndian>
#include <qindaqt/services/display_color_model/color_limits.h>

#include <cstring>

namespace QindaQt::DisplayColor::Testing
{

// Builds a minimal but structurally valid ICC file: the 128-byte header, a
// one-entry tag table, a 'desc' (textDescriptionType) tag payload carrying
// description, and zero padding up to profileSize. Tests that need hostile
// files mutate the returned bytes or truncate them.
inline QByteArray buildIccFileBytes(quint32 profileSize, const QString &description,
                                    const QByteArray &deviceClass = "mntr",
                                    const QByteArray &dataColorSpace = "RGB ",
                                    const QByteArray &connectionSpace = "XYZ ")
{
    QByteArray file;
    file.resize(static_cast<qsizetype>(profileSize));
    file.fill('\0');

    uchar *base = reinterpret_cast<uchar *>(file.data());
    const quint32 sizeBe = qToBigEndian(profileSize);
    std::memcpy(base, &sizeBe, 4);
    const quint32 cmmBe = qToBigEndian(0x4150504c); // 'APPL'
    std::memcpy(base + 4, &cmmBe, 4);
    const quint32 versionBe = qToBigEndian(0x02400000);
    std::memcpy(base + 8, &versionBe, 4);
    std::memcpy(base + 12, deviceClass.constData(), 4);
    std::memcpy(base + 16, dataColorSpace.constData(), 4);
    std::memcpy(base + 20, connectionSpace.constData(), 4);
    const quint32 magicBe = qToBigEndian(IccMagicAcsp);
    std::memcpy(base + 36, &magicBe, 4);
    for (int i = 0; i < 16; ++i) {
        base[84 + i] = static_cast<uchar>(i * 17);
    }

    if (description.isEmpty()) {
        return file;
    }

    const QByteArray utf8 = description.toUtf8();
    const quint32 descPayloadSize = 12 + static_cast<quint32>(utf8.size()) + 1;
    const quint32 tagTableOffset = IccHeaderSizeBytes;
    const quint32 descPayloadOffset = tagTableOffset + 4 + 12;

    const quint32 tagCountBe = qToBigEndian(quint32{1});
    std::memcpy(base + tagTableOffset, &tagCountBe, 4);
    const quint32 signatureBe = qToBigEndian(0x64657363); // 'desc'
    std::memcpy(base + tagTableOffset + 4, &signatureBe, 4);
    const quint32 offsetBe = qToBigEndian(descPayloadOffset);
    std::memcpy(base + tagTableOffset + 8, &offsetBe, 4);
    const quint32 lengthBe = qToBigEndian(descPayloadSize);
    std::memcpy(base + tagTableOffset + 12, &lengthBe, 4);

    std::memcpy(base + descPayloadOffset, &signatureBe, 4);
    const quint32 asciiCountBe = qToBigEndian(static_cast<quint32>(utf8.size()) + 1); // includes terminator
    std::memcpy(base + descPayloadOffset + 8, &asciiCountBe, 4);
    std::memcpy(base + descPayloadOffset + 12, utf8.constData(),
                static_cast<size_t>(utf8.size()));
    base[descPayloadOffset + 12 + utf8.size()] = 0;
    return file;
}

// Builds an ICC file whose description tag is a v4 'mluc'
// (multiLocalizedUnicode) record holding UTF-16BE text.
inline QByteArray buildMlucIccFileBytes(quint32 profileSize, const QString &description)
{
    QByteArray file = buildIccFileBytes(profileSize, QString());
    uchar *base = reinterpret_cast<uchar *>(file.data());

    // UTF-16BE text payload.
    const QString text = description;
    QByteArray utf16Be;
    utf16Be.reserve(text.size() * 2);
    for (const QChar ch : text) {
        const quint16 unit = qToBigEndian(ch.unicode());
        utf16Be.append(reinterpret_cast<const char *>(&unit), 2);
    }

    const quint32 mlucHeaderSize = 16;
    const quint32 recordSize = 12;
    const quint32 tagPayloadSize = mlucHeaderSize + recordSize +
                                   static_cast<quint32>(utf16Be.size());
    const quint32 tagTableOffset = IccHeaderSizeBytes;
    const quint32 tagPayloadOffset = tagTableOffset + 4 + 12;
    const quint32 recordTextOffset = mlucHeaderSize + recordSize;

    const quint32 tagCountBe = qToBigEndian(quint32{1});
    std::memcpy(base + tagTableOffset, &tagCountBe, 4);
    const quint32 signatureBe = qToBigEndian(0x6d6c7563); // 'mluc'
    std::memcpy(base + tagTableOffset + 4, &signatureBe, 4);
    const quint32 offsetBe = qToBigEndian(tagPayloadOffset);
    std::memcpy(base + tagTableOffset + 8, &offsetBe, 4);
    const quint32 lengthBe = qToBigEndian(tagPayloadSize);
    std::memcpy(base + tagTableOffset + 12, &lengthBe, 4);

    std::memcpy(base + tagPayloadOffset, &signatureBe, 4);
    const quint32 recordCountBe = qToBigEndian(quint32{1});
    std::memcpy(base + tagPayloadOffset + 8, &recordCountBe, 4);
    const quint32 recordSizeBe = qToBigEndian(recordSize);
    std::memcpy(base + tagPayloadOffset + 12, &recordSizeBe, 4);
    // First record: language 'en', country 'US', length, text offset.
    base[tagPayloadOffset + 16] = 'e';
    base[tagPayloadOffset + 17] = 'n';
    base[tagPayloadOffset + 18] = 'U';
    base[tagPayloadOffset + 19] = 'S';
    const quint32 textLengthBe = qToBigEndian(static_cast<quint32>(utf16Be.size()));
    std::memcpy(base + tagPayloadOffset + 20, &textLengthBe, 4);
    const quint32 textOffsetBe = qToBigEndian(recordTextOffset);
    std::memcpy(base + tagPayloadOffset + 24, &textOffsetBe, 4);
    std::memcpy(base + tagPayloadOffset + recordTextOffset, utf16Be.constData(),
                static_cast<size_t>(utf16Be.size()));
    return file;
}

inline bool writeFileBytes(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(content) == content.size();
}

} // namespace QindaQt::DisplayColor::Testing
