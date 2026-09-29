// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Graph I/O for per-device latency offsets (ADR-0288): PipeWire node Props
// `latencyOffsetNsec` and its PropInfo range. Worker-thread only, like every
// other WirePlumber handle (ADR-0014).

#include "latency_policy_p.h"

#include <wp/wp.h>

#include <optional>

namespace QindaQt::Audio::WirePlumberLatency
{

// The node's current offset from WirePlumber's Props cache, which needs
// WP_PIPEWIRE_OBJECT_FEATURE_PARAM_PROPS; absent when it publishes none.
[[nodiscard]] std::optional<qint64> readOffset(WpPipewireObject *node);
// The declared latencyOffsetNsec range in a PropInfo enumeration result;
// absent when the node declares no such range.
[[nodiscard]] std::optional<LatencyPolicy::Range> parseRange(WpIterator *propInfo);
// Writes Props { latencyOffsetNsec }. False when WirePlumber refused the call;
// the node's echo, not this result, is what later snapshots report.
bool writeOffset(WpPipewireObject *node, qint64 offsetNs);

} // namespace QindaQt::Audio::WirePlumberLatency
