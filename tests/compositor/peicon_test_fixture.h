// SPDX-License-Identifier: GPL-3.0-or-later

// Shared fixture builders for the ADR-0230 PE-icon tests. Every byte of the
// synthetic PE images is laid out here explicitly so the fixtures stay
// readable and reviewable; hostile variants are one knob each. No fixture
// bytes come from a real executable.

#pragma once

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QList>

namespace PeFixture {

struct IconResource {
    quint32 id = 0;
    QByteArray payload; // one RT_ICON payload (PNG-in-ICO or DIB)
};

struct GroupResource {
    quint32 id = 0;
    QByteArray payload; // one RT_GROUP_ICON payload (GRPICONDIR)
};

struct Options {
    QList<IconResource> icons;
    QList<GroupResource> groups;
    quint16 optionalMagic = 0x10B;   // PE32; 0x20B exercises PE32+
    bool mzMagic = true;
    bool peSignature = true;
    quint32 eLfanew = 0x80;
    int sectionCount = 1;            // >96 triggers the section cap
    bool sectionRawBeyondEnd = false; // hostile: raw data past EOF
    quint32 resourceRva = 0x1000;    // 0: "no resource directory"
    bool resourceRvaOutsideSections = false;
    qint64 truncateTo = -1;          // truncate the final image to this size
};

// Little-endian writers at explicit offsets.
inline void putU16(QByteArray &into, qsizetype offset, quint16 value)
{
    into[offset] = char(value & 0xFF);
    into[offset + 1] = char((value >> 8) & 0xFF);
}

inline void putU32(QByteArray &into, qsizetype offset, quint32 value)
{
    into[offset] = char(value & 0xFF);
    into[offset + 1] = char((value >> 8) & 0xFF);
    into[offset + 2] = char((value >> 16) & 0xFF);
    into[offset + 3] = char((value >> 24) & 0xFF);
}

// One GRPICONENTRY (14 bytes) with every field caller-controlled.
inline void appendGroupEntry(QByteArray &into, quint8 width, quint8 height,
                             quint16 bitCount, quint32 bytesInResource,
                             quint16 iconId)
{
    const qsizetype base = into.size();
    into.append(14, char(0));
    into[base] = char(width);
    into[base + 1] = char(height);
    // bColorCount and bReserved stay zero.
    putU16(into, base + 4, 1); // wPlanes
    putU16(into, base + 6, bitCount);
    putU32(into, base + 8, bytesInResource);
    putU16(into, base + 12, iconId);
}

// A well-formed GRPICONDIR header; entries are appended by the caller so a
// test can declare a count that does not match the bytes on purpose.
inline QByteArray groupDirectory(quint16 declaredCount)
{
    QByteArray payload;
    payload.append(6, char(0));
    putU16(payload, 0, 0); // idReserved
    putU16(payload, 2, 1); // idType: icons
    putU16(payload, 4, declaredCount);
    return payload;
}

// A valid PNG-in-ICO payload: a real solid-colour PNG built with QImage.
inline QByteArray pngPayload(int width, int height, QRgb color)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

// A 32bpp BMP-in-ICO payload: BITMAPINFOHEADER (40 bytes), XOR pixels
// (bottom-up BGRA), then the 1bpp AND mask. alphaOf steers the "zero alpha
// means opaque" quirk; maskedPixels flips AND-mask bits for those x.
inline QByteArray dibPayload32(int width, int height, QRgb color,
                               bool zeroAlpha, const QList<int> &maskedPixels = {})
{
    QByteArray payload;
    payload.append(40, char(0));
    putU32(payload, 0, 40);                 // biSize
    putU32(payload, 4, quint32(width));     // biWidth
    putU32(payload, 8, quint32(height * 2)); // biHeight (doubled)
    putU16(payload, 12, 1);                 // biPlanes
    putU16(payload, 14, 32);                // biBitCount
    putU32(payload, 16, 0);                 // biCompression = BI_RGB
    const qint64 stride = qint64(width) * 4;
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            payload.append(char(qBlue(color)));
            payload.append(char(qGreen(color)));
            payload.append(char(qRed(color)));
            payload.append(char(zeroAlpha ? 0 : qAlpha(color)));
        }
    }
    const qint64 maskStride = ((qint64(width) + 31) / 32) * 4;
    for (int y = 0; y < height; ++y) {
        QByteArray row(int(maskStride), char(0));
        for (const int x : maskedPixels) {
            if (x >= 0 && x < width) {
                row[x / 8] = char(quint8(row[x / 8]) | (0x80 >> (x % 8)));
            }
        }
        payload.append(row);
    }
    Q_UNUSED(stride);
    return payload;
}

// A 24bpp BMP-in-ICO payload (no alpha; the AND mask owns transparency).
inline QByteArray dibPayload24(int width, int height, QRgb color)
{
    QByteArray payload;
    payload.append(40, char(0));
    putU32(payload, 0, 40);
    putU32(payload, 4, quint32(width));
    putU32(payload, 8, quint32(height * 2));
    putU16(payload, 12, 1);
    putU16(payload, 14, 24);
    putU32(payload, 16, 0);
    const qint64 stride = ((qint64(width) * 24 + 31) / 32) * 4;
    for (int y = height - 1; y >= 0; --y) {
        QByteArray row(int(stride), char(0));
        for (int x = 0; x < width; ++x) {
            row[x * 3] = char(qBlue(color));
            row[x * 3 + 1] = char(qGreen(color));
            row[x * 3 + 2] = char(qRed(color));
        }
        payload.append(row);
    }
    const qint64 maskStride = ((qint64(width) + 31) / 32) * 4;
    payload.append(QByteArray(int(maskStride * height), char(0)));
    return payload;
}

// One resource directory header plus caller-supplied entries.
// Entries are (id, target); targets with the high bit set are subdirectories.
inline QByteArray resourceDirectory(const QList<QPair<quint32, quint32>> &entries)
{
    QByteArray directory;
    directory.append(16, char(0));
    putU16(directory, 12, 0);                     // NumberOfNamedEntries
    putU16(directory, 14, quint16(entries.size())); // NumberOfIdEntries
    for (const auto &entry : entries) {
        QByteArray raw(8, char(0));
        putU32(raw, 0, entry.first);
        putU32(raw, 4, entry.second);
        directory.append(raw);
    }
    return directory;
}

inline QByteArray dataEntry(quint32 dataRva, quint32 size)
{
    QByteArray entry(16, char(0));
    putU32(entry, 0, dataRva);
    putU32(entry, 4, size);
    return entry;
}

// The .rsrc section contents: root directory, one name directory per group
// and icon, one language directory each, the data entries, then payloads.
// Returned together with the payload RVAs so callers can assert selection.
inline QByteArray buildResourceSection(const Options &options)
{
    QByteArray section;
    const quint32 subdirectory = 0x80000000u;

    // AGENT-GUARD: a PE resource tree has ONE directory per level, not one per
    // resource. RT_ICON is a single name directory holding every icon id; the
    // root's type entry points at that one directory. Building a separate
    // one-entry directory per icon left every icon after the first
    // unreachable, because nothing pointed at it — which made the three
    // selection rows silently compare against icon id 1 forever.
    //
    // Layout: root, the group name directory, the icon name directory, one
    // language directory per resource, one data entry per resource, payloads.
    const int rootEntries = (options.groups.isEmpty() ? 0 : 1)
        + (options.icons.isEmpty() ? 0 : 1);
    const qint64 groupCount = options.groups.size();
    const qint64 iconCount = options.icons.size();
    const qint64 rootSize = 16 + qint64(rootEntries) * 8;
    const qint64 groupNameDirOffset = rootSize;
    const qint64 groupNameDirSize = groupCount == 0 ? 0 : 16 + groupCount * 8;
    const qint64 iconNameDirOffset = groupNameDirOffset + groupNameDirSize;
    const qint64 iconNameDirSize = iconCount == 0 ? 0 : 16 + iconCount * 8;
    const qint64 langDirsOffset = iconNameDirOffset + iconNameDirSize;
    const qint64 resourceCount = groupCount + iconCount;
    const qint64 dataEntriesOffset = langDirsOffset + resourceCount * 24;
    const qint64 payloadsOffset = dataEntriesOffset + resourceCount * 16;

    QList<QPair<quint32, quint32>> root;
    if (groupCount > 0) {
        root.append({14, subdirectory | quint32(groupNameDirOffset)});
    }
    if (iconCount > 0) {
        root.append({3, subdirectory | quint32(iconNameDirOffset)});
    }
    section.append(resourceDirectory(root));

    // One name directory for RT_GROUP_ICON, every group id in it. Language
    // directory `g` belongs to resource `g`, groups first then icons.
    if (groupCount > 0) {
        QList<QPair<quint32, quint32>> names;
        for (int g = 0; g < groupCount; ++g) {
            names.append({options.groups.at(g).id,
                          subdirectory | quint32(langDirsOffset + g * 24)});
        }
        section.append(resourceDirectory(names));
    }
    // One name directory for RT_ICON, every icon id in it.
    if (iconCount > 0) {
        QList<QPair<quint32, quint32>> names;
        for (int i = 0; i < iconCount; ++i) {
            names.append({options.icons.at(i).id,
                          subdirectory
                              | quint32(langDirsOffset + (groupCount + i) * 24)});
        }
        section.append(resourceDirectory(names));
    }
    // Language directories: language 0 pointing at this resource's data entry.
    for (int r = 0; r < resourceCount; ++r) {
        section.append(
            resourceDirectory({{0, quint32(dataEntriesOffset + r * 16)}}));
    }
    // Data entries, then payloads, both group-first then icons.
    qint64 payloadCursor = payloadsOffset;
    for (int g = 0; g < groupCount; ++g) {
        section.append(dataEntry(options.resourceRva + quint32(payloadCursor),
                                 quint32(options.groups.at(g).payload.size())));
        payloadCursor += options.groups.at(g).payload.size();
    }
    for (int i = 0; i < iconCount; ++i) {
        section.append(dataEntry(options.resourceRva + quint32(payloadCursor),
                                 quint32(options.icons.at(i).payload.size())));
        payloadCursor += options.icons.at(i).payload.size();
    }
    for (const GroupResource &group : options.groups) {
        section.append(group.payload);
    }
    for (const IconResource &icon : options.icons) {
        section.append(icon.payload);
    }
    return section;
}

// The whole synthetic PE image.
inline QByteArray buildPe(const Options &options)
{
    const QByteArray resourceSection = buildResourceSection(options);
    const qint64 sectionRawOffset = 0x200;
    const quint32 optionalSize = 0xF0; // PE32 standard size

    // AGENT-GUARD: zero-fill explicitly. Qt 6's QByteArray::resize leaves the
    // new bytes UNINITIALISED, and this builder relies on every field it does
    // not write being zero -- including the two NUL bytes of the PE signature
    // and every reserved COFF field. With a garbage heap the parser saw
    // "PE\xc8\xff" and rejected every valid image, so all ten happy-path
    // rows failed while all nineteen rejection rows passed.
    QByteArray image(int(sectionRawOffset), '\0');
    // DOS header.
    if (options.mzMagic) {
        image[0] = 'M';
        image[1] = 'Z';
    }
    putU32(image, 0x3C, options.eLfanew);

    // NT headers may sit beyond the current buffer for hostile e_lfanew
    // values; only write what fits, the parser must reject the rest.
    const qint64 nt = options.eLfanew;
    if (nt + 24 + optionalSize <= image.size()) {
        if (options.peSignature) {
            image[nt] = 'P';
            image[nt + 1] = 'E';
        }
        putU16(image, nt + 4, 0x8664);                    // Machine
        putU16(image, nt + 6, quint16(options.sectionCount));
        putU16(image, nt + 20, optionalSize);
        const qint64 opt = nt + 24;
        putU16(image, opt, options.optionalMagic);
        // PE32 keeps 16 data directories at offset 96, PE32+ at 112.
        const qint64 directoryOffset = options.optionalMagic == 0x20B ? 112 : 96;
        putU32(image, opt + (options.optionalMagic == 0x20B ? 108 : 92), 16);
        // Data directory entry 2 (resources).
        const quint32 reportedRva = options.resourceRvaOutsideSections
            ? 0x70000000u
            : options.resourceRva;
        putU32(image, opt + directoryOffset + 16, reportedRva);
        putU32(image, opt + directoryOffset + 20, quint32(resourceSection.size()));

        // One section (.rsrc) at RVA 0x1000 holding the resource tree.
        const qint64 sectionTable = opt + optionalSize;
        if (sectionTable + 40 <= image.size()) {
            image[sectionTable] = '.';
            image[sectionTable + 1] = 'r';
            putU32(image, sectionTable + 8, quint32(resourceSection.size()));
            putU32(image, sectionTable + 12, 0x1000);
            const quint32 rawSize = options.sectionRawBeyondEnd
                ? quint32(resourceSection.size()) * 16
                : quint32(resourceSection.size());
            putU32(image, sectionTable + 16, rawSize);
            putU32(image, sectionTable + 20, quint32(sectionRawOffset));
        }
    }
    image.append(resourceSection);
    if (options.truncateTo >= 0 && options.truncateTo < image.size()) {
        image.truncate(options.truncateTo);
    }
    return image;
}

} // namespace PeFixture
