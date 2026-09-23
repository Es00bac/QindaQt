// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Audio
{

inline constexpr quint32 kSchemaVersion = 12;
inline constexpr qsizetype kMaxOutputs = 128;
inline constexpr qsizetype kMaxInputs = 128;
inline constexpr qsizetype kMaxStreams = 256;
inline constexpr qsizetype kMaxDisplayNameUtf8Bytes = 256;
// PipeWire `node.name`: the device's stable identity (schema 4). Unlike a
// serial it survives the daemon restarting and the machine rebooting, which is
// what lets a console remember WHICH microphone a strip was set to.
inline constexpr qsizetype kMaxNodeNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxApplicationNameUtf8Bytes = 256;
inline constexpr qsizetype kMaxReasonCodeUtf8Bytes = 64;
inline constexpr qsizetype kMaxDiagnosticUtf8Bytes = 512;
inline constexpr qsizetype kMaxInFlightOperations = 64;
inline constexpr qsizetype kMaxChannelsPerDevice = 32;
inline constexpr qsizetype kMaxChannelNameUtf8Bytes = 24;
inline constexpr qsizetype kMaxVirtualNameUtf8Bytes = 96;

// Console bounds (ADR-0173). The reference console offers 5 hardware and 3
// virtual input strips against 5 physical and 3 virtual output buses; these
// ceilings leave room above that without letting a hostile snapshot allocate
// without limit. The matrix is bounded by construction: every strip carries at
// most one send per published bus.
inline constexpr qsizetype kMaxStrips = 32;
inline constexpr qsizetype kMaxBuses = 16;
inline constexpr qsizetype kMaxSendsPerStrip = kMaxBuses;
inline constexpr qsizetype kMaxConsoleIdUtf8Bytes = 64;
inline constexpr qsizetype kMaxConsoleLabelUtf8Bytes = 128;

// A meter reads dBFS, where 0 is full scale. This floor is the "no signal"
// value a console shows at rest; it is NOT the fader's kMinGainDb, and the two
// scales must never be interchanged.
inline constexpr double kSilentMeterDb = -96.0;
inline constexpr double kMaxMeterDb = 0.0;
// Pan travel, hard left to hard right.
inline constexpr double kMinPan = -1.0;
inline constexpr double kMaxPan = 1.0;

// AGENT-CONTRACT: Only nodes whose node.name carries this prefix may be
// destroyed through RemoveVirtualDevice. The WirePlumber adapter refuses to
// request destruction of any other node, so hardware can never be removed
// through Audio1.
inline constexpr char kVirtualDeviceNamePrefix[] = "qindaqt.virtual.";

inline constexpr char kServiceName[] = "org.qindaqt.Audio1";
inline constexpr char kObjectPath[] = "/org/qindaqt/Audio1";
inline constexpr char kInterfaceName[] = "org.qindaqt.Audio1";

// Strip processing bounds (ADR-0179). Every processor parameter the console
// publishes is checked against these, so a client can draw a control with a
// known range and the graph is never handed a value the plugin cannot take.
inline constexpr double kMinThresholdDb = -80.0;
inline constexpr double kMaxThresholdDb = 0.0;
inline constexpr double kMinTimeMs = 0.0;
inline constexpr double kMaxTimeMs = 2000.0;
inline constexpr double kMinRatio = 1.0;
inline constexpr double kMaxRatio = 20.0;
inline constexpr double kMinEqGainDb = -24.0;
inline constexpr double kMaxEqGainDb = 24.0;
inline constexpr double kMinEqHz = 20.0;
inline constexpr double kMaxEqHz = 20000.0;
inline constexpr double kMinEqQ = 0.1;
inline constexpr double kMaxEqQ = 10.0;
inline constexpr double kMinRangeDb = -90.0;
// Presets (ADR-0182): named copies of the console document.
inline constexpr qsizetype kMaxPresets = 64;
inline constexpr qsizetype kMaxPresetNameUtf8Bytes = 64;
// Macro buttons (ADR-0183): named sequences of console operations.
inline constexpr qsizetype kMaxMacros = 32;
inline constexpr qsizetype kMaxMacroActions = 16;
inline constexpr qsizetype kMaxMacroNameUtf8Bytes = 64;
// The recorder (ADR-0184).
inline constexpr qsizetype kMaxRecordingPathUtf8Bytes = 1024;
// VBAN (ADR-0185): network audio streams defined in a user-owned document.
inline constexpr qsizetype kMaxVbanStreams = 16;
inline constexpr qsizetype kMaxVbanNameUtf8Bytes = 16;
inline constexpr qsizetype kMaxVbanHostUtf8Bytes = 253;
inline constexpr qsizetype kMaxVbanOutputNodeUtf8Bytes = 253;

} // namespace QindaQt::Audio
