// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/power_applet/power_applet_types.h>

namespace QindaQt::Shell::PowerApplet {

// Formats a bounded nonnegative second count as a complete human duration
// phrase such as "2 hours 5 minutes" or "under a minute". Values above the
// Power1 estimate bound or negative input yield an empty string so callers
// can omit the sentence entirely.
[[nodiscard]] QString formatTimeRemaining(qint64 seconds);

// Pure, reentrant presentation projection over one complete borrowed PB-0
// generation. AGENT-CONTRACT: this function consumes public Power1 values and
// an optionally injected composed brightness model only. It must never call
// validation into question by repairing numbers, never estimate time or
// severity, never own transport or policy, and never trust rows past the
// Power1 collection bounds. Owner loss on either lane fails that lane closed
// instead of publishing stale truth.
[[nodiscard]] PowerAppletModel
projectPowerApplet(const Power::Snapshot &snapshot, bool powerOwnerAvailable,
                   const BrightnessView &brightness);

} // namespace QindaQt::Shell::PowerApplet
