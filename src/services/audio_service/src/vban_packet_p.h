// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <cstdint>
#include <optional>

namespace QindaQt::Audio::Vban
{

// VBAN (ADR-0185): the reference console's network audio, an open UDP
// protocol. A packet is a 28-byte header followed by interleaved PCM. This
// file is the whole wire format; nothing else in the service knows a byte of
// it, and the header codec is pure so its tests need no socket.
constexpr int kHeaderBytes = 28;
constexpr int kMaxSamplesPerFrame = 256;
constexpr int kMaxChannels = 8;
constexpr int kMaxPacketBytes = kHeaderBytes + kMaxSamplesPerFrame * kMaxChannels * 4;
constexpr int kStreamNameBytes = 16;
constexpr uint16_t kDefaultPort = 6980;

// Sample-rate table index (the protocol's own list); we speak 48 kHz only.
constexpr uint8_t kRate48000Index = 3;
// Data type codes (low three bits of the format byte).
constexpr uint8_t kFormatPcm16 = 1;
constexpr uint8_t kFormatFloat32 = 4;

struct Header {
    uint32_t sampleRate = 48000;
    // 1..256: the wire format stores the value minus one, so 256 is a real
    // count and needs the wider type (a uint8_t wraps 256 back to 0).
    uint16_t samplesPerFrame = 0;
    uint8_t channels = 0;          // 1..8
    uint8_t dataType = kFormatPcm16;
    QString streamName;            // at most 16 bytes, ASCII
    uint32_t frameCounter = 0;
};

// Encodes the header; the stream name is truncated to 16 bytes and padded.
[[nodiscard]] QByteArray encodeHeader(const Header &header);
// Decodes a header from the front of a datagram. nullopt when the magic is
// wrong, the rate is not 48 kHz, the type is not PCM16 or float32, or the
// counts are out of range.
[[nodiscard]] std::optional<Header> decodeHeader(const QByteArray &datagram);
// Bytes per sample for a data type we accept.
[[nodiscard]] int bytesPerSample(uint8_t dataType);

} // namespace QindaQt::Audio::Vban
