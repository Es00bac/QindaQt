// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_applet_model.h"

#include <QtCore/QObject>

#include <algorithm>
#include <cmath>

namespace QindaQt::Shell::AudioApplet {

namespace {

QString deviceLabel(const Audio::Device &device)
{
    // The description is the user-facing string; the short name is the
    // fallback. Both are bounded, sanitized Audio1 text, so presentation
    // never re-sanitizes here.
    if (!device.description.isEmpty())
        return device.description;
    if (!device.name.isEmpty())
        return device.name;
    return QObject::tr("Unknown device");
}

QString streamLabel(const Audio::Stream &stream)
{
    if (!stream.applicationName.isEmpty())
        return stream.applicationName;
    if (!stream.mediaName.isEmpty())
        return stream.mediaName;
    return QObject::tr("Unknown stream");
}

const Audio::Device *findDevice(const Audio::Snapshot &snapshot,
                                const Audio::Handle &handle)
{
    if (!handle.isValid())
        return nullptr;
    for (const Audio::Device &device : snapshot.outputs)
        if (device.handle == handle)
            return &device;
    for (const Audio::Device &device : snapshot.inputs)
        if (device.handle == handle)
            return &device;
    return nullptr;
}

DeviceRow projectDevice(const Audio::Device &device, bool pending)
{
    DeviceRow row;
    row.m_serial = device.handle.serial;
    row.m_label = deviceLabel(device);
    row.m_isOutput = device.kind == Audio::DeviceKind::Output;
    row.m_isDefault = device.isDefault;
    row.m_volume = device.volumeKnown ? device.volume : 0.0;
    row.m_volumeKnown = device.volumeKnown;
    row.m_muted = device.muted;
    row.m_muteKnown = device.muteKnown;
    row.m_canSetVolume = device.canSetVolume;
    row.m_canSetMute = device.canSetMute;
    row.m_pending = pending;
    return row;
}

StreamRow projectStream(const Audio::Stream &stream, bool pending)
{
    StreamRow row;
    row.m_serial = stream.handle.serial;
    row.m_label = streamLabel(stream);
    row.m_isPlayback = stream.direction == Audio::StreamDirection::Playback;
    row.m_volume = stream.volumeKnown ? stream.volume : 0.0;
    row.m_volumeKnown = stream.volumeKnown;
    row.m_muted = stream.muted;
    row.m_muteKnown = stream.muteKnown;
    row.m_canSetVolume = stream.canSetVolume;
    row.m_canSetMute = stream.canSetMute;
    row.m_pending = pending;
    return row;
}

} // namespace

std::optional<double>
AudioAppletModel::clampVolumeLevel(double volume) noexcept
{
    if (!std::isfinite(volume))
        return std::nullopt;
    return std::clamp(volume, 0.0, 1.0);
}

AudioAppletModel AudioAppletModel::project(Phase phase,
                                           const QString &phaseReasonCode,
                                           const Audio::Snapshot *snapshot,
                                           const QSet<quint64> &pendingSerials)
{
    AudioAppletModel model;
    model.m_phase = phase;
    model.m_phaseReasonCode = phaseReasonCode;

    if (snapshot == nullptr) {
        // No validated snapshot exists yet (for example while the client is
        // starting). Keep the caller's phase; there is simply nothing to show.
        return model;
    }
    if (!snapshot->wireValid) {
        // AGENT-GUARD: fail closed with no rows when the wire payload was
        // invalid, regardless of the caller-supplied phase.
        model.m_phase = Phase::Unavailable;
        model.m_phaseReasonCode = QStringLiteral("malformed-snapshot");
        return model;
    }

    // Presentation keeps only rows within the bounded window, in the
    // protocol's ascending-serial order, outputs before inputs. The default
    // labels stay correct even when the default device falls outside the
    // window; the overflow count explains what was hidden.
    const int deviceBudget = qMax(0, kMaxDeviceRows);
    for (const Audio::Device &device : snapshot->outputs) {
        if (model.m_deviceRows.size() >= deviceBudget)
            break;
        model.m_deviceRows.append(
            projectDevice(device, pendingSerials.contains(device.handle.serial)));
    }
    for (const Audio::Device &device : snapshot->inputs) {
        if (model.m_deviceRows.size() >= deviceBudget)
            break;
        model.m_deviceRows.append(
            projectDevice(device, pendingSerials.contains(device.handle.serial)));
    }
    const int totalDevices = static_cast<int>(snapshot->outputs.size())
        + static_cast<int>(snapshot->inputs.size());
    model.m_overflowDeviceCount = qMax(
        0, totalDevices - static_cast<int>(model.m_deviceRows.size()));

    const int streamBudget = qMax(0, kMaxStreamRows);
    for (const Audio::Stream &stream : snapshot->streams) {
        if (model.m_streamRows.size() >= streamBudget)
            break;
        model.m_streamRows.append(
            projectStream(stream, pendingSerials.contains(stream.handle.serial)));
    }
    model.m_overflowStreamCount = qMax(
        0, static_cast<int>(snapshot->streams.size())
            - static_cast<int>(model.m_streamRows.size()));

    if (const Audio::Device *output =
            findDevice(*snapshot, snapshot->defaultOutput))
        model.m_defaultOutputLabel = deviceLabel(*output);
    if (const Audio::Device *input =
            findDevice(*snapshot, snapshot->defaultInput))
        model.m_defaultInputLabel = deviceLabel(*input);

    return model;
}

} // namespace QindaQt::Shell::AudioApplet
