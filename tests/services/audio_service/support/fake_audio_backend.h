// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/audio_backend.h>

#include <algorithm>

namespace QindaQt::Tests
{

class FakeAudioBackend final : public Audio::AudioBackend
{
public:
    using AudioBackend::AudioBackend;

    quint64 start() override
    {
        ++startCalls;
        ++generation;
        if (generation == 0) {
            ++generation;
        }
        running = true;
        return generation;
    }
    void stop() override
    {
        ++stopCalls;
        running = false;
    }
    void submit(const quint64 operationId,
                const Audio::OperationRequest &request) override
    {
        operations.push_back({operationId, request});
    }

    void applyRouting(const QList<Audio::BackendRoutingEdge> &edges) override
    {
        routing = edges;
        ++routingCalls;
    }
    void applyMetering(const QList<Audio::BackendMeterTarget> &targets) override
    {
        metering = targets;
        ++meteringCalls;
    }

    void publishLevels(const QList<Audio::LevelReading> &levels)
    {
        Q_EMIT levelsReady(generation, levels);
    }
    void publishLevelsForGeneration(const quint64 runGeneration,
                                    const QList<Audio::LevelReading> &levels)
    {
        Q_EMIT levelsReady(runGeneration, levels);
    }

    void publish(const Audio::Snapshot &snapshot)
    {
        publishForGeneration(generation, snapshot);
    }
    void publishForGeneration(const quint64 runGeneration,
                              const Audio::Snapshot &snapshot)
    {
        Q_EMIT snapshotReady(runGeneration, snapshot);
    }
    void finish(const quint64 operationId, const Audio::BackendOperationOutcome &outcome)
    {
        finishForGeneration(generation, operationId, outcome);
    }
    void finishForGeneration(const quint64 runGeneration, const quint64 operationId,
                             const Audio::BackendOperationOutcome &outcome)
    {
        Q_EMIT operationFinished(runGeneration, operationId, outcome);
    }

    struct RecordedOperation {
        quint64 operationId = 0;
        Audio::OperationRequest request;
    };

    QList<RecordedOperation> operations;
    QList<Audio::BackendRoutingEdge> routing;
    QList<Audio::BackendMeterTarget> metering;
    int routingCalls = 0;
    int meteringCalls = 0;
    int startCalls = 0;
    int stopCalls = 0;
    quint64 generation = 0;
    bool running = false;
};

// The capability bits the coordinator adds because IT owns the console; the
// graph backend never publishes them (ADR-0173).
inline Audio::Capabilities consoleCapabilityBits()
{
    return Audio::Capabilities{} | Audio::Capability::Console
        | Audio::Capability::SetConsoleGain | Audio::Capability::SetConsoleRouting
        | Audio::Capability::ConsoleMeters;
}

inline Audio::Snapshot audioSnapshot(const quint64 epoch = 7,
                                     const quint64 revision = 3)
{
    Audio::Snapshot snapshot;
    snapshot.epoch = epoch;
    snapshot.revision = revision;
    snapshot.availability = Audio::Availability::Ready;
    snapshot.capabilities = Audio::Capability::SetDefault
        | Audio::Capability::SetVolume | Audio::Capability::SetMute
        | Audio::Capability::MoveStream | Audio::Capability::SetChannelVolumes
        | Audio::Capability::ManageVirtualDevices;
    snapshot.defaultOutput = {.epoch = epoch, .serial = 10};
    snapshot.defaultInput = {.epoch = epoch, .serial = 20};
    snapshot.outputs = {{.handle = {.epoch = epoch, .serial = 10},
                         .kind = Audio::DeviceKind::Output,
                         .name = QStringLiteral("Output"),
                         .description = {},
                         .volume = 0.5,
                         .volumeKnown = true,
                         .muted = false,
                         .muteKnown = true,
                         .isDefault = true,
                         .canSetVolume = true,
                         .canSetMute = true,
                         .channelVolumes = {0.5, 0.5},
                         .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")},
                         .virtualDevice = false},
                        {.handle = {.epoch = epoch, .serial = 11},
                         .kind = Audio::DeviceKind::Output,
                         .name = QStringLiteral("Virtual Output"),
                         .description = {},
                         .volume = 0.5,
                         .volumeKnown = true,
                         .muted = false,
                         .muteKnown = true,
                         .isDefault = false,
                         .canSetVolume = true,
                         .canSetMute = true,
                         .channelVolumes = {0.25, 0.75},
                         .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")},
                         .virtualDevice = true}};
    snapshot.inputs = {{.handle = {.epoch = epoch, .serial = 20},
                        .kind = Audio::DeviceKind::Input,
                        .name = QStringLiteral("Input"),
                        .description = {},
                        .volume = 0.5,
                        .volumeKnown = true,
                        .muted = false,
                        .muteKnown = true,
                        .isDefault = true,
                        .canSetVolume = true,
                        .canSetMute = true,
                        .channelVolumes = {0.5, 0.5},
                        .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")},
                        .virtualDevice = false}};
    snapshot.streams = {{.handle = {.epoch = epoch, .serial = 30},
                         .direction = Audio::StreamDirection::Playback,
                         .applicationName = QStringLiteral("Player"),
                         .mediaName = QStringLiteral("Music"),
                         .target = {.epoch = epoch, .serial = 10},
                         .targetKnown = true,
                         .volume = 0.75,
                         .volumeKnown = true,
                         .muted = false,
                         .muteKnown = true,
                         .canSetVolume = true,
                         .canSetMute = true,
                         .canMove = true,
                         .channelVolumes = {0.75, 0.75},
                         .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")}}};
    return snapshot;
}

// What the coordinator actually publishes for a given backend snapshot: the
// graph the backend described, plus the console the coordinator owns and the
// capabilities that console adds. Tests compare against THIS rather than the
// raw backend value, because a coordinator that dropped the user's console on
// every graph publication would otherwise look correct.
inline Audio::Snapshot publishedSnapshot(const Audio::Snapshot &backendSnapshot,
                                         const Audio::Console &console)
{
    Audio::Snapshot expected = backendSnapshot;
    expected.console = console;
    expected.capabilities |= consoleCapabilityBits();
    return expected;
}

// Simulates the backend completing a CreateVirtualDevice: emits the outcome
// and publishes a snapshot with the newly created managed virtual device
// appended, mirroring what the WirePlumber adapter observes after the fence.
inline void fulfillVirtualDeviceCreation(FakeAudioBackend &backend,
                                         const quint64 operationId, const quint64 epoch,
                                         const quint64 revision, const quint64 serial)
{
    Audio::Snapshot snapshot = audioSnapshot(epoch, revision);
    snapshot.outputs.push_back({.handle = {.epoch = epoch, .serial = serial},
                                .kind = Audio::DeviceKind::Output,
                                .name = QStringLiteral("Virtual Output 2"),
                                .description = {},
                                .volume = 1.0,
                                .volumeKnown = true,
                                .muted = false,
                                .muteKnown = true,
                                .isDefault = false,
                                .canSetVolume = true,
                                .canSetMute = true,
                                .channelVolumes = {1.0, 1.0},
                                .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")},
                                .virtualDevice = true});
    std::sort(snapshot.outputs.begin(), snapshot.outputs.end(),
              [](const Audio::Device &left, const Audio::Device &right) {
                  return left.handle.serial < right.handle.serial;
              });
    backend.finish(operationId,
                   {.status = Audio::BackendOperationStatus::Succeeded,
                    .reasonCode = QStringLiteral("ok"),
                    .diagnostic = {}});
    backend.publish(snapshot);
}

} // namespace QindaQt::Tests
