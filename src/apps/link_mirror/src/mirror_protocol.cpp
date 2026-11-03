// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/link_mirror/mirror_protocol.h"

#include <QtEndian>

namespace qindaqt::link_mirror {

bool validStreamSize(qint64 width, qint64 height)
{
    return width >= 1 && height >= 1 && width <= kMaxStreamSide && height <= kMaxStreamSide
           && width * height <= kMaxStreamPixels;
}

std::uint32_t maxPayloadFor(std::uint8_t channel)
{
    switch (channel) {
    case static_cast<std::uint8_t>(Channel::Control):
    case static_cast<std::uint8_t>(Channel::Inventory):
        return kMaxControlPayload;
    case static_cast<std::uint8_t>(Channel::Video):
        return kMaxVideoPayload;
    default:
        return 0;
    }
}

QByteArray encodeFrame(Channel channel, std::uint8_t type, QByteArrayView payload)
{
    const auto limit = maxPayloadFor(static_cast<std::uint8_t>(channel));
    if (payload.isEmpty() || static_cast<std::uint64_t>(payload.size()) > limit) {
        return {};
    }
    QByteArray frame(kHeaderSize, '\0');
    frame[0] = static_cast<char>(channel);
    frame[1] = static_cast<char>(type);
    qToBigEndian(static_cast<std::uint32_t>(payload.size()), frame.data() + 4);
    frame.append(payload);
    return frame;
}

QString decodeStatusName(DecodeStatus status)
{
    switch (status) {
    case DecodeStatus::Frame:
        return QStringLiteral("frame");
    case DecodeStatus::NeedsMore:
        return QStringLiteral("needs-more");
    case DecodeStatus::BadChannel:
        return QStringLiteral("bad-channel");
    case DecodeStatus::BadReserved:
        return QStringLiteral("bad-reserved");
    case DecodeStatus::BadLength:
        return QStringLiteral("bad-length");
    }
    return QStringLiteral("unknown");
}

void FrameDecoder::append(QByteArrayView bytes)
{
    if (m_error) {
        return;
    }
    // Compact consumed bytes before growing so a long session does not keep every frame.
    if (m_offset > 0 && m_offset >= m_buffer.size() / 2) {
        m_buffer.remove(0, m_offset);
        m_offset = 0;
    }
    m_buffer.append(bytes);
}

DecodeStatus FrameDecoder::next(Frame &out)
{
    if (m_error) {
        return *m_error;
    }
    const qsizetype available = m_buffer.size() - m_offset;
    if (available < kHeaderSize) {
        return DecodeStatus::NeedsMore;
    }
    const auto *header = reinterpret_cast<const unsigned char *>(m_buffer.constData() + m_offset);
    // AGENT-GUARD: check order (channel, reserved, length) matches Frames.read() in the Java
    // server so both sides classify the same malformed bytes identically.
    const std::uint32_t limit = maxPayloadFor(header[0]);
    if (limit == 0) {
        m_error = DecodeStatus::BadChannel;
        return *m_error;
    }
    if (header[2] != 0 || header[3] != 0) {
        m_error = DecodeStatus::BadReserved;
        return *m_error;
    }
    const std::uint32_t length = qFromBigEndian<std::uint32_t>(header + 4);
    if (length < 1 || length > limit) {
        m_error = DecodeStatus::BadLength;
        return *m_error;
    }
    const qsizetype total = kHeaderSize + static_cast<qsizetype>(length);
    if (available < total) {
        return DecodeStatus::NeedsMore;
    }
    out.channel = static_cast<Channel>(header[0]);
    out.type = header[1];
    out.payload = m_buffer.mid(m_offset + kHeaderSize, static_cast<qsizetype>(length));
    m_offset += total;
    if (m_offset == m_buffer.size()) {
        m_buffer.clear();
        m_offset = 0;
    }
    return DecodeStatus::Frame;
}

std::optional<VideoPacket> parseVideoPacket(const QByteArray &payload)
{
    if (payload.size() <= kPacketHeaderSize) {
        return std::nullopt;
    }
    const auto *bytes = reinterpret_cast<const unsigned char *>(payload.constData());
    const std::uint8_t flags = bytes[0];
    if ((flags & ~(video::FlagConfig | video::FlagKey)) != 0) {
        return std::nullopt;
    }
    VideoPacket packet;
    packet.config = (flags & video::FlagConfig) != 0;
    packet.key = (flags & video::FlagKey) != 0;
    packet.ptsUs = qFromBigEndian<std::int64_t>(bytes + 1);
    packet.data = payload.mid(kPacketHeaderSize);
    return packet;
}

QByteArray encodeVideoPacket(const VideoPacket &packet)
{
    QByteArray payload(kPacketHeaderSize, '\0');
    std::uint8_t flags = 0;
    if (packet.config) {
        flags |= video::FlagConfig;
    }
    if (packet.key) {
        flags |= video::FlagKey;
    }
    payload[0] = static_cast<char>(flags);
    qToBigEndian(packet.ptsUs, payload.data() + 1);
    payload.append(packet.data);
    return payload;
}

} // namespace qindaqt::link_mirror
