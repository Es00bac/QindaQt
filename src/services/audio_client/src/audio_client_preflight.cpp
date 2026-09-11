// SPDX-License-Identifier: LGPL-3.0-or-later

#include "audio_client_preflight_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <cmath>

namespace QindaQt::Audio
{
namespace
{

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
    const bool targeted = request.kind != OperationKind::CreateVirtualDevice;
    if (targeted && (!request.primary.isValid() || request.primary.epoch != snapshot.epoch)) {
        return QStringLiteral("stale-handle");
    }
    const Device *device = findDevice(snapshot, request.primary);
    const Stream *stream = findStream(snapshot, request.primary);
    switch (request.kind) {
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
