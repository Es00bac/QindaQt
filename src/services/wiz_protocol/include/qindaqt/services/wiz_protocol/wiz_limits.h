// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Wiz
{

// AGENT-CONTRACT: Every bound here is enforced by the decoder before a value
// reaches the model. A luminaire is an unauthenticated LAN peer that anyone on
// the same broadcast domain can impersonate, so decoded inventory must stay
// bounded no matter what it sends.
namespace Limits
{

// The vendor's control port. Devices answer unicast and broadcast on it and
// send their own notifications from it.
inline constexpr quint16 controlPort = 38899;

// A device reply is a single small JSON object. Anything larger is refused
// without parsing.
inline constexpr int maximumDatagramBytes = 4096;

// Upper bound on tracked luminaires. Discovery is a broadcast, so a hostile
// peer can answer from many source addresses; the model stops admitting new
// devices past this count rather than growing without limit.
inline constexpr int maximumDevices = 64;

// Bounds for free-form strings a device supplies (module name, firmware).
inline constexpr int maximumTextLength = 64;

// Stored configuration bounds. These are ours, not the device's.
inline constexpr int maximumPresets = 64;
inline constexpr int maximumLabelLength = 48;

// Brightness the vendor firmware accepts. Values below the floor are rejected
// by the device with an error rather than being clamped, so the model refuses
// them locally instead of issuing a request that cannot succeed.
inline constexpr int minimumDimmingPercent = 10;
inline constexpr int maximumDimmingPercent = 100;

// Absolute white-temperature envelope. A device narrows this from its own
// model configuration; nothing outside it is ever accepted.
inline constexpr int minimumKelvin = 1000;
inline constexpr int maximumKelvin = 10000;

// Dynamic-scene playback speed, in percent, as the firmware defines it.
inline constexpr int minimumSpeedPercent = 10;
inline constexpr int maximumSpeedPercent = 200;

// Highest scene identifier the vendor has assigned, plus the out-of-band
// rhythm identifier that is not part of the contiguous range.
inline constexpr int maximumSceneId = 32;
inline constexpr int rhythmSceneId = 1000;

} // namespace Limits

} // namespace QindaQt::Wiz
