// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/services/android_runtime/runtime_port.h"
#include <QByteArray>
#include <optional>
#include <variant>
namespace QindaQt::Services::AndroidRuntime {
inline constexpr qsizetype maxPacketBytes = 65536;
enum class PacketKind : quint8 { Snapshot = 1, Refresh = 2, Launch = 3, Result = 4 };
struct Packet {
    PacketKind kind = PacketKind::Refresh;
    QByteArray channelKey;
    std::variant<std::monostate, RuntimeSnapshot,
                 LaunchRequest, LaunchResult> value;
};
// Fixed version1 little-endian packets; bounded UTF-8 before allocation,
// closed tags, no trailing bytes or partial snapshots. Strings/arrays own data.
// A channel key is private bootstrap data, never a credential in a public API.
[[nodiscard]] QByteArray encodePacket(const Packet &packet);
[[nodiscard]] std::optional<Packet> decodePacket(const QByteArray &bytes);
} // namespace QindaQt::Services::AndroidRuntime
