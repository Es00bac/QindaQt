// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::ObsBridge {

// Registers the two OBS source types (ADR-0208): "QindaQt Console Bus" and
// "QindaQt Console Strip". Each wraps one private PulseAudio-protocol capture
// source pinned to the console node named in its settings and republishes
// that audio as its own, so OBS meters, mixes and records the console
// endpoint under the console's name. Safe to call once per process after
// obs_startup().
void registerConsoleSourceTypes();

} // namespace QindaQt::ObsBridge
