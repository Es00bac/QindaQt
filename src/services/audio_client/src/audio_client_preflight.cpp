// SPDX-License-Identifier: LGPL-3.0-or-later

#include "audio_client_preflight_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <cmath>

namespace QindaQt::Audio
{
namespace
{

// A console operation needs the console slice plus the right mutator bit; the
// gain and routing bits are separate so a service can publish a read-only
// console (ADR-0173).
[[nodiscard]] bool hasConsoleCapability(Capabilities capabilities)
{
    return capabilities.testFlag(Capability::Console)
        && (capabilities.testFlag(Capability::SetConsoleGain)
            || capabilities.testFlag(Capability::SetConsoleRouting));
}

const Device *findDevice(const Snapshot &snapshot, const Handle &handle)
{
    for (const Device &device : snapshot.outputs) {
        if (device.handle == handle) {
            return &device;
        }
    }
    for (const Device &device : snapshot.inputs) {
        if (device.handle == handle) {
            return &device;
        }
    }
    return nullptr;
}

const Stream *findStream(const Snapshot &snapshot, const Handle &handle)
{
    for (const Stream &stream : snapshot.streams) {
        if (stream.handle == handle) {
            return &stream;
        }
    }
    return nullptr;
}

} // namespace

QString preflightOperation(const Snapshot &snapshot, const OperationRequest &request)
{
    if (snapshot.availability != Availability::Ready
        && snapshot.availability != Availability::Degraded) {
        return QStringLiteral("unavailable");
    }
    // An invalid handle on the two pin kinds means "back to automatic" and is
    // admitted (ADR-0178); a valid one is checked like any other.
    const bool clearsPin = (request.kind == OperationKind::SetStripSource
                            || request.kind == OperationKind::SetBusTarget)
        && !request.primary.isValid();
    const bool targeted = operationTargetsHandle(request.kind) && !clearsPin;
    if (targeted && (!request.primary.isValid() || request.primary.epoch != snapshot.epoch)) {
        return QStringLiteral("stale-handle");
    }
    const Device *device = findDevice(snapshot, request.primary);
    const Stream *stream = findStream(snapshot, request.primary);
    switch (request.kind) {
    case OperationKind::SetStripGain:
    case OperationKind::SetStripMute:
    case OperationKind::SetStripSolo:
    case OperationKind::SetStripMono:
    case OperationKind::SetStripPan:
    case OperationKind::SetStripTrim:
    case OperationKind::SetStripSend:
    case OperationKind::SetBusGain:
    case OperationKind::SetBusMute:
    case OperationKind::SetBusMono:
    case OperationKind::SetBusTarget:
    case OperationKind::SetStripSource:
    case OperationKind::SetStripProcessing:
        // Console operations (ADR-0173) are admitted by the console model,
        // which owns the strip and bus identities they name. There is no
        // device or stream handle here to pre-check against the snapshot.
        if (!hasConsoleCapability(snapshot.capabilities)) {
            return QStringLiteral("unsupported");
        }
        return QString{};
    case OperationKind::SetDefault:
        if (!snapshot.capabilities.testFlag(Capability::SetDefault)) {
            return QStringLiteral("unsupported");
        }
        return device == nullptr ? QStringLiteral("stale-handle") : QString{};
    case OperationKind::SetVolume:
        if (!std::isfinite(request.volume) || request.volume < 0.0
            || request.volume > 1.0) {
            return QStringLiteral("invalid-volume");
        }
        if (device == nullptr && stream == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!snapshot.capabilities.testFlag(Capability::SetVolume)
            || (device != nullptr && !device->canSetVolume)
            || (stream != nullptr && !stream->canSetVolume)) {
            return QStringLiteral("unsupported");
        }
        return {};
    case OperationKind::SetChannelVolumes: {
        if (device == nullptr && stream == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!snapshot.capabilities.testFlag(Capability::SetChannelVolumes)
            || (device != nullptr && !device->canSetVolume)
            || (stream != nullptr && !stream->canSetVolume)) {
            return QStringLiteral("unsupported");
        }
        if (request.channelVolumes.size() > kMaxChannelsPerDevice) {
            return QStringLiteral("invalid-volume");
        }
        for (const double level : request.channelVolumes) {
            if (!std::isfinite(level) || level < 0.0 || level > 1.0) {
                return QStringLiteral("invalid-volume");
            }
        }
        const qsizetype retainedChannels = device != nullptr
            ? device->channelVolumes.size()
            : stream->channelVolumes.size();
        if (retainedChannels == 0 || request.channelVolumes.size() != retainedChannels) {
            return QStringLiteral("invalid-target");
        }
        return {};
    }
    case OperationKind::SetMute:
        if (device == nullptr && stream == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!snapshot.capabilities.testFlag(Capability::SetMute)
            || (device != nullptr && !device->canSetMute)
            || (stream != nullptr && !stream->canSetMute)) {
            return QStringLiteral("unsupported");
        }
        return {};
    case OperationKind::MoveStream: {
        if (!request.secondary.isValid() || request.secondary.epoch != snapshot.epoch) {
            return QStringLiteral("stale-handle");
        }
        const Device *target = findDevice(snapshot, request.secondary);
        if (stream == nullptr || target == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!snapshot.capabilities.testFlag(Capability::MoveStream)
            || !stream->canMove) {
            return QStringLiteral("unsupported");
        }
        const bool compatible = stream->direction == StreamDirection::Playback
            ? target->kind == DeviceKind::Output
            : target->kind == DeviceKind::Input;
        return compatible ? QString{} : QStringLiteral("incompatible-target");
    }
    case OperationKind::CreateVirtualDevice:
        if (!snapshot.capabilities.testFlag(Capability::ManageVirtualDevices)) {
            return QStringLiteral("unsupported");
        }
        if (request.deviceKind != DeviceKind::Output
            && request.deviceKind != DeviceKind::Input) {
            return QStringLiteral("malformed-request");
        }
        if (request.displayName.isEmpty()
            || !isBoundedText(request.displayName, kMaxVirtualNameUtf8Bytes)) {
            return QStringLiteral("invalid-name");
        }
        if (request.channels != 2 && request.channels != 4 && request.channels != 6
            && request.channels != 8) {
            return QStringLiteral("invalid-channel-count");
        }
        return {};
    case OperationKind::RemoveVirtualDevice: {
        if (device == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!snapshot.capabilities.testFlag(Capability::ManageVirtualDevices)) {
            return QStringLiteral("unsupported");
        }
        // AGENT-GUARD: refuse to remove any device the service does not
        // manage; the backend enforces the same fence against node names.
        if (!device->virtualDevice) {
            return QStringLiteral("invalid-target");
        }
        return {};
    }
    }
    return QStringLiteral("malformed-request");
}

} // namespace QindaQt::Audio
