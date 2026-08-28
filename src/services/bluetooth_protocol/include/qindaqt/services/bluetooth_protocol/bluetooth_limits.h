// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Bluetooth
{

inline constexpr quint32 kSchemaVersion = 1;
inline constexpr qsizetype kMaxAdapters = 16;
inline constexpr qsizetype kMaxDevices = 256;
inline constexpr qsizetype kMaxAddressUtf8Bytes = 32;
inline constexpr qsizetype kMaxNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxReasonCodeUtf8Bytes = 64;
inline constexpr qsizetype kMaxDiagnosticUtf8Bytes = 512;
inline constexpr qsizetype kMaxInFlightOperations = 64;

inline constexpr char kServiceName[] = "org.qindaqt.Bluetooth1";
inline constexpr char kObjectPath[] = "/org/qindaqt/Bluetooth1";
inline constexpr char kInterfaceName[] = "org.qindaqt.Bluetooth1";

} // namespace QindaQt::Bluetooth
