// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Audio
{

inline constexpr quint32 kSchemaVersion = 2;
inline constexpr qsizetype kMaxOutputs = 128;
inline constexpr qsizetype kMaxInputs = 128;
inline constexpr qsizetype kMaxStreams = 256;
inline constexpr qsizetype kMaxDisplayNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxApplicationNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxReasonCodeUtf8Bytes = 64;
inline constexpr qsizetype kMaxDiagnosticUtf8Bytes = 512;
inline constexpr qsizetype kMaxInFlightOperations = 64;
inline constexpr qsizetype kMaxChannelsPerDevice = 32;
inline constexpr qsizetype kMaxChannelNameUtf8Bytes = 24;
inline constexpr qsizetype kMaxVirtualNameUtf8Bytes = 96;

// AGENT-CONTRACT: Only nodes whose node.name carries this prefix may be
// destroyed through RemoveVirtualDevice. The WirePlumber adapter refuses to
// request destruction of any other node, so hardware can never be removed
// through Audio1.
inline constexpr char kVirtualDeviceNamePrefix[] = "qindaqt.virtual.";

inline constexpr char kServiceName[] = "org.qindaqt.Audio1";
inline constexpr char kObjectPath[] = "/org/qindaqt/Audio1";
inline constexpr char kInterfaceName[] = "org.qindaqt.Audio1";

} // namespace QindaQt::Audio
