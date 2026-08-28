// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Network {

inline constexpr quint32 kProtocolVersion = 1;
inline constexpr char kServiceName[] = "org.qindaqt.Network1";
inline constexpr char kObjectPath[] = "/org/qindaqt/Network1";
inline constexpr char kInterfaceName[] = "org.qindaqt.Network1";

// AGENT-CONTRACT: These collection caps are the accepted Network1 v1 value
// limits. A decoder consumes hostile input into a temporary and rejects the
// whole value when any count exceeds its cap; it never publishes a prefix.
inline constexpr qsizetype kMaxRadios = 4;
inline constexpr qsizetype kMaxDevices = 8;
inline constexpr qsizetype kMaxAccessPoints = 64;
inline constexpr qsizetype kMaxKnownNetworks = 128;
inline constexpr qsizetype kMaxActiveConnections = 8;
inline constexpr qsizetype kMaxSerializedBytes = 1'048'576;

// IEEE 802.11 SSIDs are at most 32 octets. Longer raw SSIDs are rejected
// rather than truncated, so a spoofed advertisement can never collapse onto a
// different real network identity.
inline constexpr qsizetype kMaxSsidRawBytes = 32;
inline constexpr qsizetype kMaxSsidUtf8Bytes = kMaxSsidRawBytes * 4;
inline constexpr qsizetype kMaxBssidUtf8Bytes = 17;
// Linux interface names are limited to IFNAMSIZ-1 printable octets.
inline constexpr qsizetype kMaxInterfaceUtf8Bytes = 15;
inline constexpr qsizetype kMaxNetworkIdUtf8Bytes = 64;
inline constexpr qsizetype kMaxLeaseIdUtf8Bytes = 64;
inline constexpr qsizetype kMaxReasonCodeUtf8Bytes = 64;
inline constexpr qsizetype kMaxDiagnosticUtf8Bytes = 512;
// A D-Bus unique bus name is bounded well below this by the reference broker.
inline constexpr qsizetype kMaxOwnerUtf8Bytes = 255;

inline constexpr quint32 kMaximumSignalStrength = 100;
// Zero means "frequency unknown"; otherwise only 2.4/5/6 GHz WLAN channels are
// representable in v1. Wired devices leave the field at zero.
inline constexpr quint32 kMinimumWlanFrequencyMHz = 2'412;
inline constexpr quint32 kMaximumWlanFrequencyMHz = 7'125;

// Scan leases and request deadlines are bounded so a hostile or crashed peer
// can neither pin a scan forever nor force sub-millisecond retry storms.
inline constexpr qint64 kMinimumScanDeadlineMilliseconds = 1'000;
inline constexpr qint64 kMaximumScanDeadlineMilliseconds = 120'000;
inline constexpr int kMinimumRequestTimeoutMilliseconds = 100;
inline constexpr int kMaximumRequestTimeoutMilliseconds = 60'000;
inline constexpr int kMaximumRetryDelayMilliseconds = 60'000;

// Canonical byte-codec identity. Any byte-layout change requires a new
// accepted codec/protocol version; decoders never guess a layout.
inline constexpr char kSnapshotMagic[] = {'Q', 'N', '1', 'S'};
inline constexpr char kOperationResultMagic[] = {'Q', 'N', '1', 'R'};
inline constexpr quint32 kCanonicalCodecVersion = 1;

} // namespace QindaQt::Network
