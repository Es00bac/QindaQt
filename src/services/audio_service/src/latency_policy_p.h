// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Pure latency-offset policy (ADR-0288): how a graph reading becomes the
// schema-13 device fields, and when the graph worker writes a remembered
// offset onto a node. No PipeWire or WirePlumber types, so the rules are
// testable without a daemon; wireplumber_latency_p.h does the graph I/O.

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <optional>
#include <unordered_map>

namespace QindaQt::Audio::LatencyPolicy
{

// A node's declared settable range for latencyOffsetNsec (its PropInfo),
// before Audio1 clips it to the protocol window.
struct Range {
    qint64 minNs = 0;
    qint64 maxNs = 0;
    friend bool operator==(const Range &, const Range &) = default;
};

// Per node serial: the queried range, or nullopt when the node declares none.
// A serial missing from the map has not been queried yet.
using Ranges = std::unordered_map<quint64, std::optional<Range>>;

// Fills the schema-13 latency fields of `device` from what the graph reported:
// `observed` is the node's Props value (absent when it publishes none) and
// `declared` its PropInfo range (absent until queried, or when not a range).
// AGENT-GUARD: every path leaves the fields in the canonical form
// validDeviceLatency accepts. An out-of-window reading becomes unknown, never
// a clamped number, and a device without a node.name is never settable
// because the offset could not be remembered for it.
void project(Device &device, std::optional<qint64> observed,
             std::optional<Range> declared, bool capability);

// How many times the worker writes one declared offset onto one node before
// concluding the node keeps overriding it and leaving that truth visible.
inline constexpr int kMaxWritesPerDeclaration = 3;

// Per node serial reconcile memory, owned by the graph worker.
struct ReconcileState {
    // The declaration the write budget belongs to; a new one resets it.
    std::optional<qint64> declared;
    int writes = 0;
    // What the node reported when the last write was issued. While it still
    // reports that, the write is in flight (or was silently refused) and is
    // not repeated.
    std::optional<qint64> observedAtWrite;
};

// Decides whether `declared` should be written to a node now reporting
// `observed`, recording the attempt in `state`. A node that publishes no
// offset is never written.
[[nodiscard]] bool shouldWrite(ReconcileState &state, qint64 declared,
                               std::optional<qint64> observed);

} // namespace QindaQt::Audio::LatencyPolicy
