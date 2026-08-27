// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Audio
{

inline constexpr quint32 kSchemaVersion = 1;
inline constexpr qsizetype kMaxOutputs = 128;
inline constexpr qsizetype kMaxInputs = 128;
inline constexpr qsizetype kMaxStreams = 256;
inline constexpr qsizetype kMaxDisplayNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxApplicationNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxReasonCodeUtf8Bytes = 64;
inline constexpr qsizetype kMaxDiagnosticUtf8Bytes = 512;
inline constexpr qsizetype kMaxInFlightOperations = 64;

inline constexpr char kServiceName[] = "org.qindaqt.Audio1";
inline constexpr char kObjectPath[] = "/org/qindaqt/Audio1";
inline constexpr char kInterfaceName[] = "org.qindaqt.Audio1";

} // namespace QindaQt::Audio
