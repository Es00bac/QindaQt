// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/peicon.h"

#include <QIODevice>
#include <QMessageLogContext>

#include <cstring>

namespace QindaQt::Compositor {
namespace {

constexpr quint32 kResourceTypeIcon = 3;        // RT_ICON
constexpr quint32 kResourceTypeGroupIcon = 14;  // RT_GROUP_ICON

// Bounded seekable view over the device. Every read validates its range
// against the real size first; arithmetic is qint64 throughout so a 32-bit
// length field can never overflow a check.
class BoundedReader
{
public:
    BoundedReader(QIODevice &device, qint64 size) : m_device(device), m_size(size) {}

    [[nodiscard]] bool inRange(qint64 offset, qint64 bytes) const
    {
        return offset >= 0 && bytes >= 0 && offset <= m_size
            && bytes <= m_size - offset;
    }

    [[nodiscard]] bool read(qint64 offset, char *out, qsizetype bytes) const
    {
        if (!inRange(offset, bytes)) {
            return false;
        }
        if (bytes == 0) {
            return true;
        }
        if (!m_device.seek(offset)) {
            return false;
        }
        return m_device.read(out, bytes) == bytes;
    }

    [[nodiscard]] bool u16(qint64 offset, quint16 *value) const
    {
        char raw[2];
        if (!read(offset, raw, 2)) {
            return false;
        }
        *value = quint16(quint8(raw[0])) | (quint16(quint8(raw[1])) << 8);
        return true;
    }

    [[nodiscard]] bool u32(qint64 offset, quint32 *value) const
    {
        char raw[4];
        if (!read(offset, raw, 4)) {
            return false;
        }
        *value = quint32(quint8(raw[0])) | (quint32(quint8(raw[1])) << 8)
            | (quint32(quint8(raw[2])) << 16) | (quint32(quint8(raw[3])) << 24);
        return true;
    }

    // Bounded payload copy; refuses anything over the icon payload ceiling
    // before allocating, so a claimed length can never size an allocation.
    [[nodiscard]] bool bytes(qint64 offset, qint64 length, QByteArray *out) const
    {
        if (!inRange(offset, length) || length > kMaxIconPayloadBytes) {
            return false;
        }
        out->resize(qsizetype(length));
        return read(offset, out->data(), qsizetype(length));
    }

private:
    QIODevice &m_device;
    qint64 m_size;
};

struct Section {
    quint32 virtualAddress = 0;
    quint32 virtualSize = 0;
    quint32 rawOffset = 0;
    quint32 rawSize = 0;
};

// File offset for `length` bytes at `rva`, or false. The mapping must be
// backed by real section bytes; a tail only virtually present (VirtualSize
// beyond SizeOfRawData) fails closed.
[[nodiscard]] bool rvaToOffset(const QList<Section> &sections, quint32 rva,
                               qint64 length, const BoundedReader &reader,
                               qint64 *offset)
{
    for (const Section &section : sections) {
        if (rva < section.virtualAddress) {
            continue;
        }
        const qint64 delta = qint64(rva) - section.virtualAddress;
        if (delta < 0 || delta + length > section.rawSize) {
            continue;
        }
        const qint64 candidate = qint64(section.rawOffset) + delta;
        if (!reader.inRange(candidate, length)) {
            return false;
        }
        *offset = candidate;
        return true;
    }
    return false;
}

// Resource-directory offsets are 31-bit relative to the resource base; the
// sum can exceed 32 bits, so it is computed wide and refused, never wrapped.
[[nodiscard]] bool resourceStructureOffset(const BoundedReader &reader,
                                           const QList<Section> &sections,
                                           quint32 resourceBase,
                                           qint64 relativeOffset, qint64 length,
                                           qint64 *offset)
{
    const qint64 rva = qint64(resourceBase) + relativeOffset;
    if (rva < 0 || rva > qint64(0xFFFFFFFFu)) {
        return false;
    }
    return rvaToOffset(sections, quint32(rva), length, reader, offset);
}

struct IconDataEntry {
    quint32 type = 0;
    quint32 nameId = 0;
    quint32 languageId = 0;
    quint32 dataRva = 0;
    quint32 dataSize = 0;
};

struct ResourceWalk {
    const BoundedReader &reader;
    const QList<Section> &sections;
    quint32 resourceBase = 0;
    qint64 directoriesVisited = 0;
    QList<IconDataEntry> entries;
};

// One resource directory level. Depth 0 = type, 1 = name/id, 2 = language;
// only numeric (non-named) entries are followed - icon ids are numeric, so
// name strings are never read at all. A subdirectory pointer at the language
// level (or deeper) is malformed and fails the whole walk; combined with the
// depth cap this is also the CYCLE defense: a directory pointing back at an
// ancestor can never recurse past kMaxResourceDepth levels.
[[nodiscard]] bool walkResourceDirectory(ResourceWalk &walk, qint64 relativeOffset,
                                         int depth, quint32 type, quint32 nameId)
{
    if (depth >= kMaxResourceDepth) {
        return false;
    }
    if (++walk.directoriesVisited > kMaxResourceDirectories) {
        return false;
    }
    qint64 directoryOffset = 0;
    if (!resourceStructureOffset(walk.reader, walk.sections, walk.resourceBase,
                                 relativeOffset, 16, &directoryOffset)) {
        return false;
    }
    quint16 namedCount = 0;
    quint16 idCount = 0;
    if (!walk.reader.u16(directoryOffset + 12, &namedCount)
        || !walk.reader.u16(directoryOffset + 14, &idCount)) {
        return false;
    }
    const qint64 entryCount = qint64(namedCount) + idCount;
    if (entryCount > kMaxResourceEntriesPerDirectory) {
        return false;
    }
    for (qint64 index = 0; index < entryCount; ++index) {
        const qint64 entryOffset = directoryOffset + 16 + index * 8;
        quint32 name = 0;
        quint32 target = 0;
        if (!walk.reader.u32(entryOffset, &name)
            || !walk.reader.u32(entryOffset + 4, &target)) {
            return false;
        }
        if ((name & 0x80000000u) != 0) {
            // Named entry: irrelevant to numeric icon ids, skipped entirely.
            continue;
        }
        const quint32 id = name & 0xFFFFu;
        if ((target & 0x80000000u) != 0) {
            if (depth + 1 >= kMaxResourceDepth) {
                return false;
            }
            // At the type level only RT_ICON and RT_GROUP_ICON are followed;
            // every other subtree is skipped without a single read.
            if (depth == 0 && id != kResourceTypeIcon
                && id != kResourceTypeGroupIcon) {
                continue;
            }
            const quint32 nextType = depth == 0 ? id : type;
            const quint32 nextName = depth == 1 ? id : nameId;
            if (!walkResourceDirectory(walk, target & 0x7FFFFFFFu, depth + 1,
                                       nextType, nextName)) {
                return false;
            }
            continue;
        }
        if (depth == 0) {
            continue;
        }
        qint64 dataEntryOffset = 0;
        if (!resourceStructureOffset(walk.reader, walk.sections,
                                     walk.resourceBase, target, 16,
                                     &dataEntryOffset)) {
            return false;
        }
        quint32 dataRva = 0;
        quint32 dataSize = 0;
        if (!walk.reader.u32(dataEntryOffset, &dataRva)
            || !walk.reader.u32(dataEntryOffset + 4, &dataSize)) {
            return false;
        }
        walk.entries.append(IconDataEntry{type, nameId, id, dataRva, dataSize});
    }
    return true;
}

struct GroupIconEntry {
    int width = 0;
    int height = 0;
    int bitCount = 0;
    quint32 bytesInResource = 0;
    quint32 iconId = 0;
};

[[nodiscard]] bool parseGroupIcon(const QByteArray &payload,
                                  QList<GroupIconEntry> *entries)
{
    if (payload.size() < 6) {
        return false;
    }
    const auto at = [&payload](qsizetype index) { return quint8(payload.at(index)); };
    const quint16 reserved = quint16(at(0)) | (quint16(at(1)) << 8);
    const quint16 type = quint16(at(2)) | (quint16(at(3)) << 8);
    const quint16 count = quint16(at(4)) | (quint16(at(5)) << 8);
    if (reserved != 0 || type != 1 || count == 0 || count > kMaxGroupIconEntries) {
        return false;
    }
    if (qint64(payload.size()) < 6 + qint64(count) * 14) {
        return false;
    }
    for (quint16 index = 0; index < count; ++index) {
        const qsizetype base = 6 + qsizetype(index) * 14;
        GroupIconEntry entry;
        // A zero size byte means 256.
        entry.width = at(base) == 0 ? 256 : at(base);
        entry.height = at(base + 1) == 0 ? 256 : at(base + 1);
        const quint16 planes = quint16(at(base + 4)) | (quint16(at(base + 5)) << 8);
        const quint16 bits = quint16(at(base + 6)) | (quint16(at(base + 7)) << 8);
        entry.bitCount = bits != 0 ? int(bits) : int(planes);
        entry.bytesInResource = quint32(at(base + 8)) | (quint32(at(base + 9)) << 8)
            | (quint32(at(base + 10)) << 16) | (quint32(at(base + 11)) << 24);
        entry.iconId = quint16(at(base + 12)) | (quint16(at(base + 13)) << 8);
        entries->append(entry);
    }
    return true;
}

[[nodiscard]] bool isPngPayload(const QByteArray &payload)
{
    static const char magic[] = {char(0x89), 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    return payload.size() > 24
        && std::memcmp(payload.constData(), magic, sizeof(magic)) == 0;
}

// AGENT-NOTE: libpng reports a corrupt payload through qWarning, and this
// parser runs inside the compositor on untrusted files. The handler is
// scoped to the decode call and drops only libpng's own lines, so hostile
// executables fail silently instead of spamming (or, under
// QT_FATAL_WARNINGS, killing) the session.
class ScopedPngWarningSilencer
{
public:
    ScopedPngWarningSilencer() { s_previous = qInstallMessageHandler(filter); }
    ~ScopedPngWarningSilencer()
    {
        qInstallMessageHandler(s_previous);
        s_previous = nullptr;
    }

private:
    static void filter(QtMsgType type, const QMessageLogContext &context,
                       const QString &message)
    {
        if (!message.startsWith(QLatin1String("libpng")) && s_previous != nullptr) {
            s_previous(type, context, message);
        }
    }

    static inline QtMessageHandler s_previous = nullptr;
};

[[nodiscard]] QImage decodePngIcon(const QByteArray &payload)
{
    // IHDR dimensions without a full decode, so an absurd claim is refused
    // before the PNG plugin sees the bytes.
    const auto be32 = [&payload](qsizetype offset) {
        return (quint32(quint8(payload.at(offset))) << 24)
            | (quint32(quint8(payload.at(offset + 1))) << 16)
            | (quint32(quint8(payload.at(offset + 2))) << 8)
            | quint32(quint8(payload.at(offset + 3)));
    };
    const quint32 width = be32(16);
    const quint32 height = be32(20);
    if (width == 0 || height == 0 || width > kMaxIconDimension
        || height > kMaxIconDimension || qint64(width) * height > kMaxIconPixels) {
        return {};
    }
    QImage image;
    {
        ScopedPngWarningSilencer silence;
        image = QImage::fromData(payload, "PNG");
    }
    if (image.isNull() || image.width() > kMaxIconDimension
        || image.height() > kMaxIconDimension) {
        return {};
    }
    return image;
}

// BMP-in-ICO: a BITMAPINFOHEADER (exactly 40 bytes) followed by the color
// table, the XOR bitmap (bottom-up), and the 1bpp AND mask. biHeight carries
// twice the real height.
[[nodiscard]] QImage decodeBmpIcon(const QByteArray &payload)
{
    if (payload.size() < 40) {
        return {};
    }
    // AGENT-GUARD: the return type is explicit. `|` and `<<` promote their
    // operands to int, so without it the lambda yields int and every
    // assignment to a quint16 fails the repository's -Werror=conversion build.
    const auto u16 = [&payload](qsizetype offset) -> quint16 {
        return static_cast<quint16>(quint16(quint8(payload.at(offset)))
                                    | quint16(quint16(quint8(payload.at(offset + 1))) << 8));
    };
    const auto u32 = [&payload](qsizetype offset) {
        return quint32(quint8(payload.at(offset)))
            | (quint32(quint8(payload.at(offset + 1))) << 8)
            | (quint32(quint8(payload.at(offset + 2))) << 16)
            | (quint32(quint8(payload.at(offset + 3))) << 24);
    };
    const quint32 headerSize = u32(0);
    const qint64 width = u32(4);
    const qint64 doubledHeight = u32(8);
    const quint16 planes = u16(12);
    const quint16 bitCount = u16(14);
    const quint32 compression = u32(16);
    const quint32 colorsUsed = u32(32);
    if (headerSize != 40 || planes != 1 || compression != 0 || width <= 0
        || width > kMaxIconDimension || doubledHeight <= 0
        || (doubledHeight % 2) != 0) {
        return {};
    }
    const qint64 height = doubledHeight / 2;
    if (height > kMaxIconDimension || width * height > kMaxIconPixels) {
        return {};
    }
    if (bitCount != 1 && bitCount != 4 && bitCount != 8 && bitCount != 24
        && bitCount != 32) {
        return {};
    }
    qint64 paletteEntries = 0;
    if (bitCount <= 8) {
        paletteEntries = colorsUsed != 0 ? colorsUsed : (qint64(1) << bitCount);
        if (paletteEntries <= 0 || paletteEntries > 256) {
            return {};
        }
    }
    const qint64 xorOffset = 40 + paletteEntries * 4;
    const qint64 xorStride = ((width * bitCount + 31) / 32) * 4;
    const qint64 andStride = ((width + 31) / 32) * 4;
    const qint64 total = xorOffset + xorStride * height + andStride * height;
    if (total > payload.size()) {
        return {};
    }

    // 32bpp icons commonly leave the alpha channel zeroed to mean "opaque";
    // source alpha is only trusted when at least one pixel sets it.
    bool anySourceAlpha = false;
    if (bitCount == 32) {
        for (qint64 y = 0; y < height && !anySourceAlpha; ++y) {
            const qint64 rowStart = xorOffset + (height - 1 - y) * xorStride;
            for (qint64 x = 0; x < width; ++x) {
                if (quint8(payload.at(rowStart + x * 4 + 3)) != 0) {
                    anySourceAlpha = true;
                    break;
                }
            }
        }
    }
    const qint64 maskOffset = xorOffset + xorStride * height;

    QImage image(int(width), int(height), QImage::Format_ARGB32);
    if (image.isNull()) {
        return {};
    }
    for (qint64 y = 0; y < height; ++y) {
        const qint64 xorRow = xorOffset + (height - 1 - y) * xorStride;
        const qint64 andRow = maskOffset + (height - 1 - y) * andStride;
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(int(y)));
        for (qint64 x = 0; x < width; ++x) {
            const bool masked =
                (quint8(payload.at(andRow + x / 8)) & (0x80 >> (x % 8))) != 0;
            quint8 red = 0;
            quint8 green = 0;
            quint8 blue = 0;
            quint8 alpha = 255;
            if (bitCount == 32) {
                const qint64 pixel = xorRow + x * 4;
                blue = quint8(payload.at(pixel));
                green = quint8(payload.at(pixel + 1));
                red = quint8(payload.at(pixel + 2));
                // AGENT-GUARD: the source alpha is trusted only when some
                // pixel actually sets it. The presence of an AND-mask bit is
                // NOT evidence that the alpha channel is meaningful -- a
                // 32bpp icon that leaves alpha zeroed and expresses its
                // transparency through the mask is the common case, and
                // trusting the zeroed channel there made every pixel
                // transparent. The mask is applied below, independently.
                if (anySourceAlpha) {
                    alpha = quint8(payload.at(pixel + 3));
                }
            } else if (bitCount == 24) {
                const qint64 pixel = xorRow + x * 3;
                blue = quint8(payload.at(pixel));
                green = quint8(payload.at(pixel + 1));
                red = quint8(payload.at(pixel + 2));
            } else {
                quint8 paletteIndex = 0;
                if (bitCount == 8) {
                    paletteIndex = quint8(payload.at(xorRow + x));
                } else if (bitCount == 4) {
                    const quint8 packed = quint8(payload.at(xorRow + x / 2));
                    paletteIndex = (x % 2 == 0) ? (packed >> 4) : (packed & 0x0F);
                } else {
                    paletteIndex =
                        (quint8(payload.at(xorRow + x / 8)) & (0x80 >> (x % 8))) ? 1 : 0;
                }
                if (paletteIndex >= paletteEntries) {
                    return {};
                }
                const qint64 color = 40 + qint64(paletteIndex) * 4;
                blue = quint8(payload.at(color));
                green = quint8(payload.at(color + 1));
                red = quint8(payload.at(color + 2));
            }
            if (masked) {
                alpha = 0;
            }
            line[x] = qRgba(red, green, blue, alpha);
        }
    }
    return image;
}

[[nodiscard]] QImage decodeIconPayload(const QByteArray &payload)
{
    if (payload.isEmpty() || payload.size() > kMaxIconPayloadBytes) {
        return {};
    }
    if (isPngPayload(payload)) {
        return decodePngIcon(payload);
    }
    return decodeBmpIcon(payload);
}

// PNG-in-ICO entries declare bit depth 0 in the group directory; their
// payload is truecolor, so for SELECTION purposes they count as 32-bit.
[[nodiscard]] int effectiveBitCount(const GroupIconEntry &entry,
                                    const QByteArray &payload)
{
    if (entry.bitCount != 0) {
        return entry.bitCount;
    }
    return isPngPayload(payload) ? 32 : 0;
}

} // namespace

QImage extractPeIcon(QIODevice &device, qint64 size, int targetSize)
{
    const BoundedReader reader(device, size);
    // DOS header: MZ magic, then e_lfanew at 0x3C.
    char mz[2];
    quint32 ntOffset = 0;
    if (!reader.read(0, mz, 2) || mz[0] != 'M' || mz[1] != 'Z'
        || !reader.u32(0x3C, &ntOffset) || ntOffset < 64) {
        return {};
    }
    // NT signature and COFF header.
    char signature[4];
    quint16 sectionCount = 0;
    quint16 optionalHeaderSize = 0;
    if (!reader.read(ntOffset, signature, 4) || signature[0] != 'P'
        || signature[1] != 'E' || signature[2] != 0 || signature[3] != 0
        || !reader.u16(ntOffset + 6, &sectionCount) || sectionCount == 0
        || sectionCount > kMaxPeSections
        || !reader.u16(ntOffset + 20, &optionalHeaderSize)) {
        return {};
    }
    const qint64 optionalOffset = qint64(ntOffset) + 24;
    if (!reader.inRange(optionalOffset, optionalHeaderSize)
        || optionalHeaderSize < 2) {
        return {};
    }
    quint16 magic = 0;
    if (!reader.u16(optionalOffset, &magic)) {
        return {};
    }
    // Data directories: the resource directory is entry 2 (directories begin
    // at offset 96 into the PE32 optional header, 112 for PE32+).
    qint64 directoryBase = 0;
    if (magic == 0x10B) {
        directoryBase = optionalOffset + 96 + 2 * 8;
    } else if (magic == 0x20B) {
        directoryBase = optionalOffset + 112 + 2 * 8;
    } else {
        return {};
    }
    if (!reader.inRange(directoryBase, 8)
        || directoryBase + 8 > optionalOffset + optionalHeaderSize) {
        return {};
    }
    quint32 resourceRva = 0;
    if (!reader.u32(directoryBase, &resourceRva) || resourceRva == 0) {
        return {};
    }
    // The section table immediately follows the optional header.
    const qint64 sectionTableOffset = optionalOffset + optionalHeaderSize;
    if (!reader.inRange(sectionTableOffset, qint64(sectionCount) * 40)) {
        return {};
    }
    QList<Section> sections;
    sections.reserve(sectionCount);
    for (quint16 index = 0; index < sectionCount; ++index) {
        const qint64 base = sectionTableOffset + qint64(index) * 40;
        quint32 virtualSize = 0;
        quint32 virtualAddress = 0;
        quint32 rawSize = 0;
        quint32 rawOffset = 0;
        if (!reader.u32(base + 8, &virtualSize)
            || !reader.u32(base + 12, &virtualAddress)
            || !reader.u32(base + 16, &rawSize)
            || !reader.u32(base + 20, &rawOffset)) {
            return {};
        }
        // A section claiming raw bytes past the end of the file is hostile;
        // refuse the whole image rather than trusting the rest of the table.
        if (!reader.inRange(rawOffset, rawSize)) {
            return {};
        }
        sections.append(Section{virtualAddress, virtualSize, rawOffset, rawSize});
    }

    ResourceWalk walk{reader, sections, resourceRva, 0, {}};
    if (!walkResourceDirectory(walk, 0, 0, 0, 0)) {
        return {};
    }

    // The first group-icon entry wins (a real executable carries exactly
    // one); determinism matters more than language heuristics here.
    const IconDataEntry *group = nullptr;
    for (const IconDataEntry &entry : walk.entries) {
        if (entry.type == kResourceTypeGroupIcon) {
            group = &entry;
            break;
        }
    }
    if (group == nullptr || group->dataSize == 0) {
        return {};
    }
    qint64 groupOffset = 0;
    QByteArray groupPayload;
    if (!rvaToOffset(sections, group->dataRva, group->dataSize, reader, &groupOffset)
        || !reader.bytes(groupOffset, group->dataSize, &groupPayload)) {
        return {};
    }
    QList<GroupIconEntry> groupEntries;
    if (!parseGroupIcon(groupPayload, &groupEntries)) {
        return {};
    }

    // Resolve each group entry to its RT_ICON payload; first language wins
    // per icon id and a missing id disqualifies the candidate.
    struct Candidate {
        GroupIconEntry groupEntry;
        QByteArray payload;
        int effectiveBits = 0;
    };
    QList<Candidate> candidates;
    for (const GroupIconEntry &entry : groupEntries) {
        if (entry.bytesInResource == 0
            || entry.bytesInResource > kMaxIconPayloadBytes) {
            continue;
        }
        const IconDataEntry *icon = nullptr;
        for (const IconDataEntry &data : walk.entries) {
            if (data.type == kResourceTypeIcon && data.nameId == entry.iconId) {
                icon = &data;
                break;
            }
        }
        if (icon == nullptr || icon->dataSize == 0
            || icon->dataSize > kMaxIconPayloadBytes) {
            continue;
        }
        qint64 iconOffset = 0;
        QByteArray payload;
        if (!rvaToOffset(sections, icon->dataRva, icon->dataSize, reader, &iconOffset)
            || !reader.bytes(iconOffset, icon->dataSize, &payload)) {
            continue;
        }
        candidates.append(Candidate{entry, payload,
                                    effectiveBitCount(entry, payload)});
    }
    if (candidates.isEmpty()) {
        return {};
    }

    // AGENT-GUARD: ADR-0230 selection — prefer 32-bit colour; within a
    // bit-depth class take the smallest size at or above the target, else the
    // largest below. Ties take the lowest resource id (deterministic).
    const auto better = [targetSize](const Candidate &a, const Candidate &b) {
        const int aSize = qMax(a.groupEntry.width, a.groupEntry.height);
        const int bSize = qMax(b.groupEntry.width, b.groupEntry.height);
        const bool aThirtyTwo = a.effectiveBits == 32;
        const bool bThirtyTwo = b.effectiveBits == 32;
        if (aThirtyTwo != bThirtyTwo) {
            return aThirtyTwo;
        }
        const bool aAbove = aSize >= targetSize;
        const bool bAbove = bSize >= targetSize;
        if (aAbove != bAbove) {
            return aAbove;
        }
        if (aSize != bSize) {
            return aAbove ? aSize < bSize : aSize > bSize;
        }
        if (a.effectiveBits != b.effectiveBits) {
            return a.effectiveBits > b.effectiveBits;
        }
        return a.groupEntry.iconId < b.groupEntry.iconId;
    };
    const Candidate *best = &candidates.constFirst();
    for (const Candidate &candidate : candidates) {
        if (better(candidate, *best)) {
            best = &candidate;
        }
    }
    return decodeIconPayload(best->payload);
}

} // namespace QindaQt::Compositor
