// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>

namespace QindaQt::XEmbedTray
{

// AGENT-CONTRACT: argbBytes uses the exact wire layout the shell's
// StatusNotifier icon renderer validates against (premultiplied ARGB32 in
// QImage::Format_ARGB32 memory order: bytes B,G,R,A per pixel on
// little-endian hosts). Producers must keep that layout; the renderer wraps
// the buffer without any further conversion. See
// src/shell/status_notifier/icon/src/status_notifier_icon_renderer.cpp.
class TrayIconImage
{
public:
    // Capture-side ceiling, deliberately below the shell registry's 512 px /
    // 1 MiB wire ceiling: a tray icon larger than this is a broken client,
    // not an icon worth shipping over D-Bus.
    static constexpr quint32 kMaxDimension = 128;

    TrayIconImage() = default;
    TrayIconImage(quint32 width, quint32 height, QByteArray argbBytes)
        : m_width(width)
        , m_height(height)
        , m_argbBytes(std::move(argbBytes))
    {
    }

    [[nodiscard]] bool isNull() const
    {
        return m_width == 0 || m_height == 0
            || m_argbBytes.size() != qsizetype(m_width) * qsizetype(m_height) * 4;
    }
    [[nodiscard]] quint32 width() const { return m_width; }
    [[nodiscard]] quint32 height() const { return m_height; }
    [[nodiscard]] const QByteArray &argb() const { return m_argbBytes; }

    // Every alpha byte zero. Wine emits such frames transiently; publishers
    // keep the previous frame instead of presenting a blank icon.
    [[nodiscard]] bool isFullyTransparent() const;

    // Convert one XGetImage ZPixmap payload. data/size describe the raw reply
    // bytes; strideBytes is the server scanline stride; byteOrder is the
    // connection's image byte order (0 = LSBFirst, 1 = MSBFirst). Depth < 32
    // pixels are forced opaque, because a client that never drew alpha must
    // not read as invisible. Returns a null image for hostile or truncated
    // payloads, and clamps oversized captures to kMaxDimension per edge.
    [[nodiscard]] static TrayIconImage fromZPixmap(const unsigned char *data,
                                                   quint32 size,
                                                   quint32 width,
                                                   quint32 height,
                                                   quint32 strideBytes,
                                                   quint32 bitsPerPixel,
                                                   quint32 depth,
                                                   quint32 byteOrder);

private:
    quint32 m_width = 0;
    quint32 m_height = 0;
    QByteArray m_argbBytes;
};

} // namespace QindaQt::XEmbedTray
