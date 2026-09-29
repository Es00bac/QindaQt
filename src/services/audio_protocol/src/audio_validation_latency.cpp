// SPDX-License-Identifier: LGPL-3.0-or-later

// Per-device latency offset admission (schema 13, ADR-0288). One definition,
// used by snapshot validation, the Audio1 coordinator, the client preflight
// and the Settings route.
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <qindaqt/services/audio_protocol/audio_limits.h>

namespace QindaQt::Audio
{
namespace
{

[[nodiscard]] bool inWindow(const qint64 value) noexcept
{
    return value >= kMinLatencyOffsetNs && value <= kMaxLatencyOffsetNs;
}

} // namespace

bool validDeviceLatency(const Device &device, const Capabilities capabilities)
{
    // AGENT-GUARD: canonical absence. An unknown offset carries neither value
    // nor range, so no consumer can render a missing reading as a real 0 ms.
    if (!device.latencyOffsetKnown) {
        return device.latencyOffsetNs == 0 && !device.canSetLatencyOffset
            && device.latencyOffsetMinNs == 0 && device.latencyOffsetMaxNs == 0;
    }
    if (!inWindow(device.latencyOffsetNs)) {
        return false;
    }
    if (!device.canSetLatencyOffset) {
        return device.latencyOffsetMinNs == 0 && device.latencyOffsetMaxNs == 0;
    }
    return capabilities.testFlag(Capability::SetLatencyOffset)
        && inWindow(device.latencyOffsetMinNs) && inWindow(device.latencyOffsetMaxNs)
        && device.latencyOffsetMinNs <= device.latencyOffsetNs
        && device.latencyOffsetNs <= device.latencyOffsetMaxNs;
}

bool latencyOffsetAdmitted(const Device &device, const qint64 offsetNs)
{
    return device.latencyOffsetKnown && device.canSetLatencyOffset && inWindow(offsetNs)
        && offsetNs >= device.latencyOffsetMinNs && offsetNs <= device.latencyOffsetMaxNs;
}

} // namespace QindaQt::Audio
