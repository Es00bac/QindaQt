// SPDX-License-Identifier: GPL-3.0-or-later

#include "latency_policy_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <algorithm>

namespace QindaQt::Audio::LatencyPolicy
{

void project(Device &device, const std::optional<qint64> observed,
             const std::optional<Range> declared, const bool capability)
{
    device.latencyOffsetKnown = false;
    device.latencyOffsetNs = 0;
    device.canSetLatencyOffset = false;
    device.latencyOffsetMinNs = 0;
    device.latencyOffsetMaxNs = 0;
    if (!observed.has_value() || *observed < kMinLatencyOffsetNs
        || *observed > kMaxLatencyOffsetNs) {
        return;
    }
    device.latencyOffsetKnown = true;
    device.latencyOffsetNs = *observed;
    if (!capability || !declared.has_value() || device.nodeName.isEmpty()) {
        return;
    }
    const qint64 minNs = std::max(declared->minNs, kMinLatencyOffsetNs);
    const qint64 maxNs = std::min(declared->maxNs, kMaxLatencyOffsetNs);
    // A node already outside its own declared range (set by some other tool)
    // stays readable but is not offered as settable from here.
    if (minNs > maxNs || *observed < minNs || *observed > maxNs) {
        return;
    }
    device.canSetLatencyOffset = true;
    device.latencyOffsetMinNs = minNs;
    device.latencyOffsetMaxNs = maxNs;
}

bool shouldWrite(ReconcileState &state, const qint64 declared,
                 const std::optional<qint64> observed)
{
    if (!observed.has_value()) {
        return false;
    }
    if (state.declared != declared) {
        state = ReconcileState{.declared = declared, .writes = 0, .observedAtWrite = {}};
    }
    if (*observed == declared) {
        state.observedAtWrite.reset();
        return false;
    }
    // AGENT-GUARD: both fences keep this from fighting a node. The first
    // waits for the previous write's echo instead of re-sending on every
    // rebuild; the budget stops after a node reset the offset this many
    // times (a Bluetooth route re-emits its own offset once on connect).
    if (state.observedAtWrite == observed || state.writes >= kMaxWritesPerDeclaration) {
        return false;
    }
    ++state.writes;
    state.observedAtWrite = observed;
    return true;
}

} // namespace QindaQt::Audio::LatencyPolicy
