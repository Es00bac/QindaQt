// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Bluetooth
{

inline constexpr quint32 kSchemaVersion = 1;
inline constexpr qsizetype kMaxAdapters = 8;
inline constexpr qsizetype kMaxDevices = 256;
inline constexpr qsizetype kMaxAdapterNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxDeviceNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxReasonCodeUtf8Bytes = 64;
inline constexpr qsizetype kMaxDiagnosticUtf8Bytes = 512;
inline constexpr qsizetype kMaxInFlightOperations = 64;
// Discovery leases are caller-scoped and bounded in two dimensions: leases per
// adapter and total leases across the service. Both bounds are enforced by the
// model against backend-reported lease state and fail closed.
inline constexpr qsizetype kMaxDiscoveryLeasesPerAdapter = 16;
inline constexpr qsizetype kMaxDiscoveryLeasesTotal = 64;
inline constexpr qsizetype kMaxCallerIdUtf8Bytes = 64;
inline constexpr qsizetype kAddressUtf8Bytes = 17;

inline constexpr char kServiceName[] = "org.qindaqt.Bluetooth1";
inline constexpr char kObjectPath[] = "/org/qindaqt/Bluetooth1";
inline constexpr char kInterfaceName[] = "org.qindaqt.Bluetooth1";

} // namespace QindaQt::Bluetooth
