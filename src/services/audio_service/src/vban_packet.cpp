// SPDX-License-Identifier: LGPL-3.0-or-later

#include "vban_packet_p.h"

#include <cstring>

namespace QindaQt::Audio::Vban
{
namespace {

// The protocol's sample-rate list, index -> rate; we accept only 48 kHz.
constexpr uint32_t kRates[] = {6000,  12000, 24000, 48000, 96000, 192000, 384000,
                               8000,  16000, 32000, 64000, 128000, 256000, 512000,
                               11025, 22050, 44100, 88200, 176400, 352800, 705600};

} // namespace

QByteArray encodeHeader(const Header &header)
{
    QByteArray bytes(kHeaderBytes, '\0');
    std::memcpy(bytes.data(), "VBAN", 4);
    // Sub-protocol 0 (audio) in the high bits, the rate index in the low five.
    bytes[4] = static_cast<char>(kRate48000Index & 0x1F);
    bytes[5] = static_cast<char>(header.samplesPerFrame == 0 ? 0 : header.samplesPerFrame - 1);
    bytes[6] = static_cast<char>(header.channels == 0 ? 0 : header.channels - 1);
    // Codec PCM (0) in the high bits, the data type in the low three.
    bytes[7] = static_cast<char>(header.dataType & 0x07);
    const QByteArray name = header.streamName.toLatin1().left(kStreamNameBytes);
    std::memcpy(bytes.data() + 8, name.constData(), static_cast<size_t>(name.size()));
    const uint32_t counter = header.frameCounter;
    bytes[24] = static_cast<char>(counter & 0xFF);
    bytes[25] = static_cast<char>((counter >> 8) & 0xFF);
    bytes[26] = static_cast<char>((counter >> 16) & 0xFF);
    bytes[27] = static_cast<char>((counter >> 24) & 0xFF);
    return bytes;
}

std::optional<Header> decodeHeader(const QByteArray &datagram)
{
    if (datagram.size() < kHeaderBytes || std::memcmp(datagram.constData(), "VBAN", 4) != 0) {
        return std::nullopt;
    }
    const auto *bytes = reinterpret_cast<const uint8_t *>(datagram.constData());
    if ((bytes[4] >> 5) != 0) {
        return std::nullopt;   // not the audio sub-protocol
    }
    const uint8_t rateIndex = bytes[4] & 0x1F;
    if (rateIndex >= sizeof(kRates) / sizeof(kRates[0]) || kRates[rateIndex] != 48000) {
        return std::nullopt;
    }
    if ((bytes[7] >> 4) != 0) {
        return std::nullopt;   // not plain PCM
    }
    Header header;
    header.sampleRate = 48000;
    header.samplesPerFrame = static_cast<uint16_t>(bytes[5]) + 1;
    header.channels = static_cast<uint8_t>(bytes[6] + 1);
    header.dataType = bytes[7] & 0x07;
    if (header.dataType != kFormatPcm16 && header.dataType != kFormatFloat32) {
        return std::nullopt;
    }
    if (header.channels > kMaxChannels) {
        return std::nullopt;
    }
    char name[kStreamNameBytes + 1] = {};
    std::memcpy(name, datagram.constData() + 8, kStreamNameBytes);
    header.streamName = QString::fromLatin1(name);
    header.frameCounter = static_cast<uint32_t>(bytes[24]) | (static_cast<uint32_t>(bytes[25]) << 8)
        | (static_cast<uint32_t>(bytes[26]) << 16) | (static_cast<uint32_t>(bytes[27]) << 24);
    // The payload must be exactly what the header announces.
    const int expected = kHeaderBytes + header.samplesPerFrame * header.channels
        * bytesPerSample(header.dataType);
    if (datagram.size() != expected) {
        return std::nullopt;
    }
    return header;
}

int bytesPerSample(const uint8_t dataType)
{
    return dataType == kFormatFloat32 ? 4 : 2;
}

} // namespace QindaQt::Audio::Vban
