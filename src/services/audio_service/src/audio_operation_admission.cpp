// SPDX-License-Identifier: LGPL-3.0-or-later

// The coordinator's admission switch: what a request must look like against
// the retained snapshot before anything acts on it. Split from
// audio_operation_coordinator.cpp so the switch, which grows with every
// operation kind, does not push the coordinator past its source-shape budget.

#include "audio_operation_admission_p.h"

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <cmath>

namespace QindaQt::Audio
{
namespace Admission
{

bool hasConsoleCapability(Capabilities capabilities)
{
    return capabilities.testFlag(Capability::Console)
        && (capabilities.testFlag(Capability::SetConsoleGain)
            || capabilities.testFlag(Capability::SetConsoleRouting));
}

bool hasCapability(const Capabilities capabilities, const Capability capability)
{
    return capabilities.testFlag(capability);
}

const Device *findDevice(const Snapshot &snapshot, const Handle &handle)
{
    const auto inspect = [&](const QList<Device> &devices) -> const Device * {
        for (const Device &device : devices) {
            if (device.handle == handle) {
                return &device;
            }
        }
        return nullptr;
    };
    const Device *device = inspect(snapshot.outputs);
    return device == nullptr ? inspect(snapshot.inputs) : device;
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

} // namespace Admission

using namespace Admission;

namespace {

bool validRequestedVolumes(const QVector<double> &volumes)
{
    if (volumes.size() > kMaxChannelsPerDevice) {
        return false;
    }
    for (const double level : volumes) {
        if (!std::isfinite(level) || level < 0.0 || level > 1.0) {
            return false;
        }
    }
    return true;
}

bool validVirtualDeviceChannelCount(const quint32 channels)
{
    return channels == 2 || channels == 4 || channels == 6 || channels == 8;
}

} // namespace


QString AudioOperationCoordinator::validateRequest(const OperationRequest &request) const
{
    if (!m_running || (m_snapshot.availability != Availability::Ready
                       && m_snapshot.availability != Availability::Degraded)) {
        return QStringLiteral("unavailable");
    }
    const bool clearsPin = (request.kind == OperationKind::SetStripSource
                            || request.kind == OperationKind::SetBusTarget)
        && !request.primary.isValid();
    const bool targeted = operationTargetsHandle(request.kind) && !clearsPin;
    if (targeted
        && (!request.primary.isValid() || request.primary.epoch != m_snapshot.epoch)) {
        return QStringLiteral("stale-handle");
    }

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
    case OperationKind::SetBusProcessing:
    case OperationKind::SavePreset:
    case OperationKind::LoadPreset:
    case OperationKind::DeletePreset:
    case OperationKind::RunMacro:
    case OperationKind::StartRecording:
    case OperationKind::StopRecording:
    case OperationKind::SetVbanEnabled:
        // Console operations (ADR-0173) are admitted by the console model,
        // which owns the strip and bus identities they name. There is no
        // device or stream handle here to pre-check against the snapshot.
        if (!hasConsoleCapability(m_snapshot.capabilities)) {
            return QStringLiteral("unsupported");
        }
        break;
    case OperationKind::SetDefault:
        if (!hasCapability(m_snapshot.capabilities, Capability::SetDefault)) {
            return QStringLiteral("unsupported");
        }
        if (findDevice(m_snapshot, request.primary) == nullptr) {
            return QStringLiteral("stale-handle");
        }
        break;
    case OperationKind::SetVolume: {
        if (!hasCapability(m_snapshot.capabilities, Capability::SetVolume)) {
            return QStringLiteral("unsupported");
        }
        if (!std::isfinite(request.volume) || request.volume < 0.0
            || request.volume > 1.0) {
            return QStringLiteral("invalid-volume");
        }
        const Device *device = findDevice(m_snapshot, request.primary);
        const Stream *stream = findStream(m_snapshot, request.primary);
        if (device == nullptr && stream == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if ((device != nullptr && !device->canSetVolume)
            || (stream != nullptr && !stream->canSetVolume)) {
            return QStringLiteral("unsupported");
        }
        break;
    }
    case OperationKind::SetChannelVolumes: {
        if (!hasCapability(m_snapshot.capabilities, Capability::SetChannelVolumes)) {
            return QStringLiteral("unsupported");
        }
        const Device *device = findDevice(m_snapshot, request.primary);
        const Stream *stream = findStream(m_snapshot, request.primary);
        if (device == nullptr && stream == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if ((device != nullptr && !device->canSetVolume)
            || (stream != nullptr && !stream->canSetVolume)) {
            return QStringLiteral("unsupported");
        }
        if (!validRequestedVolumes(request.channelVolumes)) {
            return QStringLiteral("invalid-volume");
        }
        // AGENT-GUARD: Per-channel writes must cover exactly the retained
        // layout. A count mismatch means the request disagrees with the
        // observed channel topology; fail closed instead of writing partial
        // channel state to the mixer.
        const qsizetype retainedChannels = device != nullptr
            ? device->channelVolumes.size()
            : stream->channelVolumes.size();
        if (retainedChannels == 0 || request.channelVolumes.size() != retainedChannels) {
            return QStringLiteral("invalid-target");
        }
        break;
    }
    case OperationKind::SetMute: {
        if (!hasCapability(m_snapshot.capabilities, Capability::SetMute)) {
            return QStringLiteral("unsupported");
        }
        const Device *device = findDevice(m_snapshot, request.primary);
        const Stream *stream = findStream(m_snapshot, request.primary);
        if (device == nullptr && stream == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if ((device != nullptr && !device->canSetMute)
            || (stream != nullptr && !stream->canSetMute)) {
            return QStringLiteral("unsupported");
        }
        break;
    }
    case OperationKind::MoveStream: {
        if (!hasCapability(m_snapshot.capabilities, Capability::MoveStream)) {
            return QStringLiteral("unsupported");
        }
        if (!request.secondary.isValid() || request.secondary.epoch != m_snapshot.epoch) {
            return QStringLiteral("stale-handle");
        }
        const Stream *stream = findStream(m_snapshot, request.primary);
        const Device *target = findDevice(m_snapshot, request.secondary);
        if (stream == nullptr || target == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!stream->canMove) {
            return QStringLiteral("unsupported");
        }
        const bool compatible = stream->direction == StreamDirection::Playback
            ? target->kind == DeviceKind::Output
            : target->kind == DeviceKind::Input;
        if (!compatible) {
            return QStringLiteral("incompatible-target");
        }
        break;
    }
    case OperationKind::CreateVirtualDevice: {
        if (!hasCapability(m_snapshot.capabilities, Capability::ManageVirtualDevices)) {
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
        if (!validVirtualDeviceChannelCount(request.channels)) {
            return QStringLiteral("invalid-channel-count");
        }
        break;
    }
    case OperationKind::RemoveVirtualDevice: {
        if (!hasCapability(m_snapshot.capabilities, Capability::ManageVirtualDevices)) {
            return QStringLiteral("unsupported");
        }
        const Device *device = findDevice(m_snapshot, request.primary);
        if (device == nullptr) {
            return QStringLiteral("stale-handle");
        }
        // AGENT-GUARD: Only devices carrying the managed virtual prefix may be
        // destroyed. Removing this check would let a client destroy hardware
        // nodes through the public API.
        if (!device->virtualDevice) {
            return QStringLiteral("invalid-target");
        }
        break;
    }
    default:
        return QStringLiteral("malformed-request");
    }
    return {};
}

} // namespace QindaQt::Audio
