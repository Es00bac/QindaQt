// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Power {

inline constexpr quint32 kProtocolVersion = 1;
inline constexpr quint32 kCanonicalCodecVersion = 1;

// AGENT-CONTRACT: These collection caps are the accepted Power1 v1 wire
// limits. A decoder consumes hostile input into a temporary and rejects the
// whole value when any count exceeds its cap; it never publishes a prefix.
inline constexpr qsizetype kMaxPowerSupplies = 8;
inline constexpr qsizetype kMaxProfiles = 4;
inline constexpr qsizetype kMaxProfileHolds = 8;
inline constexpr qsizetype kMaxInhibitors = 8;
inline constexpr qsizetype kMaxKeyboardBacklights = 8;
inline constexpr qsizetype kMaxInternalBacklights = 8;
inline constexpr qsizetype kMaxSerializedBytes = 1'048'576;

inline constexpr qsizetype kMaxOpaqueIdUtf8Bytes = 128;
inline constexpr qsizetype kMaxNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxProfileIdUtf8Bytes = 64;
inline constexpr qsizetype kMaxReasonCodeUtf8Bytes = 64;
inline constexpr qsizetype kMaxDiagnosticUtf8Bytes = 512;
inline constexpr qsizetype kMaxInhibitorWhatUtf8Bytes = 128;
inline constexpr qsizetype kMaxInhibitorWhoUtf8Bytes = 256;
inline constexpr qsizetype kMaxInhibitorWhyUtf8Bytes = 512;
inline constexpr qsizetype kMaxInhibitorModeUtf8Bytes = 32;
inline constexpr qsizetype kMaxWaylandSocketUtf8Bytes = 108;

inline constexpr double kMinimumPercentage = 0.0;
inline constexpr double kMaximumPercentage = 100.0;
inline constexpr double kMaximumEnergyWattHours = 1'000'000.0;
inline constexpr double kMaximumRateWatts = 1'000'000.0;
inline constexpr qint64 kMaximumEstimateSeconds = 10LL * 365 * 24 * 60 * 60;
inline constexpr quint32 kNormalizedBrightnessMaximum = 10'000;
inline constexpr quint32 kMaximumRawBrightness = 1'000'000'000;

inline constexpr char kServiceName[] = "org.qindaqt.Power1";
inline constexpr char kObjectPath[] = "/org/qindaqt/Power1";
inline constexpr char kInterfaceName[] = "org.qindaqt.Power1";

} // namespace QindaQt::Power
