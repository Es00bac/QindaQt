// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/xembed_tray_proxy/tray_icon_image.h>

namespace QindaQt::XEmbedTray
{

namespace {

constexpr quint32 kByteOrderLsbFirst = 0;

// Read one pixel as a host-order 0xAARRGGBB word from the server payload.
// Only the layouts an X server produces for these depths are accepted:
// 32/24 bpp with 32-bit units, and 16 bpp (RGB565) for completeness.
[[nodiscard]] quint32 readPixel(const unsigned char *row,
                                quint32 x,
                                quint32 bitsPerPixel,
                                quint32 byteOrder)
{
    if (bitsPerPixel == 32) {
        const unsigned char *cell = row + x * 4;
        quint32 value;
        if (byteOrder == kByteOrderLsbFirst) {
            value = quint32(cell[0]) | (quint32(cell[1]) << 8)
                | (quint32(cell[2]) << 16) | (quint32(cell[3]) << 24);
        } else {
            value = (quint32(cell[0]) << 24) | (quint32(cell[1]) << 16)
                | (quint32(cell[2]) << 8) | quint32(cell[3]);
        }
        return value;
    }
    if (bitsPerPixel == 16) {
        const unsigned char *cell = row + x * 2;
        quint32 value;
        if (byteOrder == kByteOrderLsbFirst) {
            value = quint32(cell[0]) | (quint32(cell[1]) << 8);
        } else {
            value = (quint32(cell[0]) << 8) | quint32(cell[1]);
        }
        // RGB565 expanded to 8 bits per channel.
        const quint32 r = ((value >> 11) & 0x1F) * 255 / 31;
        const quint32 g = ((value >> 5) & 0x3F) * 255 / 63;
        const quint32 b = (value & 0x1F) * 255 / 31;
        return 0xFF000000u | (r << 16) | (g << 8) | b;
    }
    return 0xFF000000u;
}

} // namespace

bool TrayIconImage::isFullyTransparent() const
{
    if (isNull()) {
        return true;
    }
    const auto *bytes = reinterpret_cast<const unsigned char *>(m_argbBytes.constData());
    const qsizetype count = m_argbBytes.size();
    // AGENT-GUARD: alpha is the high byte of each host-order ARGB32 word,
    // which is byte 3 of each little-endian pixel cell.
    for (qsizetype i = 3; i < count; i += 4) {
        if (bytes[i] != 0) {
            return false;
        }
    }
    return true;
}

TrayIconImage TrayIconImage::fromZPixmap(const unsigned char *data,
                                         quint32 size,
                                         quint32 width,
                                         quint32 height,
                                         quint32 strideBytes,
                                         quint32 bitsPerPixel,
                                         quint32 depth,
                                         quint32 byteOrder)
{
    if (data == nullptr || width == 0 || height == 0) {
        return {};
    }
    if (bitsPerPixel != 32 && bitsPerPixel != 16) {
        return {};
    }
    const quint32 bytesPerPixel = bitsPerPixel / 8;
    if (strideBytes < width * bytesPerPixel) {
        return {};
    }
    // Reject truncation: the reply must cover every declared scanline.
    const quint64 needed = quint64(strideBytes) * quint64(height);
    if (quint64(size) < needed) {
        return {};
    }

    const quint32 outWidth = qMin(width, kMaxDimension);
    const quint32 outHeight = qMin(height, kMaxDimension);
    QByteArray out(qsizetype(outWidth) * qsizetype(outHeight) * 4, Qt::Uninitialized);
    auto *dst = reinterpret_cast<unsigned char *>(out.data());

    const bool forceOpaque = depth < 32;
    for (quint32 y = 0; y < outHeight; ++y) {
        const unsigned char *row = data + quint64(y) * strideBytes;
        unsigned char *dstRow = dst + quint64(y) * outWidth * 4;
        for (quint32 x = 0; x < outWidth; ++x) {
            quint32 pixel = readPixel(row, x, bitsPerPixel, byteOrder);
            if (forceOpaque) {
                pixel |= 0xFF000000u;
            }
            // Store as a host-order ARGB32 word: bytes B,G,R,A on
            // little-endian hosts, matching QImage::Format_ARGB32 memory.
            dstRow[x * 4 + 0] = static_cast<unsigned char>(pixel & 0xFF);
            dstRow[x * 4 + 1] = static_cast<unsigned char>((pixel >> 8) & 0xFF);
            dstRow[x * 4 + 2] = static_cast<unsigned char>((pixel >> 16) & 0xFF);
            dstRow[x * 4 + 3] = static_cast<unsigned char>((pixel >> 24) & 0xFF);
        }
    }
    return TrayIconImage(outWidth, outHeight, std::move(out));
}

} // namespace QindaQt::XEmbedTray
