// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

#include <cstdint>
#include <optional>

// Framing of QindaLink mirror protocol 1: the byte stream between qindaqt-link-mirror and the
// phone's mirror server (phone/mirror_server).
//
// AGENT-CONTRACT: docs/wiki/reference/qindalink-mirror-v1.md is normative. The Java side is
// phone/mirror_server/.../Frames.java. Changing a value here changes both files, the page and
// tests/apps/link_mirror/tst_mirror_framing.cpp in one commit.
//
// Thread-safety: pure value code. A FrameDecoder instance belongs to one thread.
namespace qindaqt::link_mirror {

inline constexpr int kProtocolVersion = 1;
inline constexpr qsizetype kHeaderSize = 8;
inline constexpr std::uint32_t kMaxControlPayload = 1U << 20;
inline constexpr std::uint32_t kMaxVideoPayload = 16U << 20;
inline constexpr qsizetype kPacketHeaderSize = 9;
inline constexpr const char *kSocketPrefix = "qindalink-mirror-";
// Stream size caps shared by message parsing and the decoder: untrusted dimensions beyond
// these are rejected before any allocation (ADR-0435).
inline constexpr int kMaxStreamSide = 8192;
inline constexpr qint64 kMaxStreamPixels = 64'000'000;

enum class Channel : std::uint8_t { Control = 0x01, Video = 0x02, Inventory = 0x03 };

namespace control {
inline constexpr std::uint8_t Hello = 0x01;
inline constexpr std::uint8_t Start = 0x02;
inline constexpr std::uint8_t Resize = 0x03;
inline constexpr std::uint8_t Touch = 0x04;
inline constexpr std::uint8_t Mouse = 0x05;
inline constexpr std::uint8_t Scroll = 0x06;
inline constexpr std::uint8_t Key = 0x07;
inline constexpr std::uint8_t Text = 0x08;
inline constexpr std::uint8_t Sync = 0x09;
inline constexpr std::uint8_t Stop = 0x0A;
inline constexpr std::uint8_t Ping = 0x0B;
inline constexpr std::uint8_t Ready = 0x81;
inline constexpr std::uint8_t Display = 0x82;
inline constexpr std::uint8_t Error = 0x83;
inline constexpr std::uint8_t AppExited = 0x84;
} // namespace control

namespace video {
inline constexpr std::uint8_t Format = 0x01;
inline constexpr std::uint8_t Packet = 0x02;
inline constexpr std::uint8_t FlagConfig = 0x01;
inline constexpr std::uint8_t FlagKey = 0x02;
} // namespace video

namespace inventory {
inline constexpr std::uint8_t List = 0x01;
inline constexpr std::uint8_t App = 0x81;
inline constexpr std::uint8_t End = 0x82;
} // namespace inventory

struct Frame {
    Channel channel = Channel::Control;
    std::uint8_t type = 0;
    QByteArray payload;
};

// True when both sides are 1..kMaxStreamSide and the area is at most kMaxStreamPixels.
[[nodiscard]] bool validStreamSize(qint64 width, qint64 height);

// Returns the payload limit for a raw channel byte, or 0 for an unknown channel.
[[nodiscard]] std::uint32_t maxPayloadFor(std::uint8_t channel);

// Encodes one frame. Returns an empty array when the payload is empty or exceeds the
// channel limit; callers treat that as a programming error.
[[nodiscard]] QByteArray encodeFrame(Channel channel, std::uint8_t type, QByteArrayView payload);

enum class DecodeStatus { Frame, NeedsMore, BadChannel, BadReserved, BadLength };

[[nodiscard]] QString decodeStatusName(DecodeStatus status);

// Incremental decoder for one connection. After any error status the decoder is failed for
// good and keeps returning that status; the caller closes the connection.
class FrameDecoder {
public:
    void append(QByteArrayView bytes);
    [[nodiscard]] DecodeStatus next(Frame &out);
    [[nodiscard]] bool failed() const { return m_error.has_value(); }
    [[nodiscard]] qsizetype buffered() const { return m_buffer.size() - m_offset; }

private:
    QByteArray m_buffer;
    qsizetype m_offset = 0;
    std::optional<DecodeStatus> m_error;
};

struct VideoPacket {
    bool config = false;
    bool key = false;
    std::int64_t ptsUs = 0;
    QByteArray data;
};

// Parses a video Packet payload. Returns nullopt when it is shorter than the packet header,
// has no bitstream bytes or sets reserved flag bits.
[[nodiscard]] std::optional<VideoPacket> parseVideoPacket(const QByteArray &payload);

// Builds a video Packet payload (the server's job; used by tests and fakes).
[[nodiscard]] QByteArray encodeVideoPacket(const VideoPacket &packet);

} // namespace qindaqt::link_mirror
