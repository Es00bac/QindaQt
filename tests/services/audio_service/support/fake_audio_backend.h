// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/audio_backend.h>

namespace QindaQt::Tests
{

class FakeAudioBackend final : public Audio::AudioBackend
{
public:
    using AudioBackend::AudioBackend;

    void start() override { ++startCalls; }
    void stop() override { ++stopCalls; }
    void submit(const quint64 operationId,
                const Audio::OperationRequest &request) override
    {
        operations.push_back({operationId, request});
    }

    void publish(const Audio::Snapshot &snapshot) { Q_EMIT snapshotReady(snapshot); }
    void finish(const quint64 operationId, const Audio::BackendOperationOutcome &outcome)
    {
        Q_EMIT operationFinished(operationId, outcome);
    }

    struct RecordedOperation {
        quint64 operationId = 0;
        Audio::OperationRequest request;
    };

    QList<RecordedOperation> operations;
    int startCalls = 0;
    int stopCalls = 0;
};

inline Audio::Snapshot audioSnapshot(const quint64 epoch = 7,
                                     const quint64 revision = 3)
{
    Audio::Snapshot snapshot;
    snapshot.epoch = epoch;
    snapshot.revision = revision;
    snapshot.availability = Audio::Availability::Ready;
    snapshot.capabilities = Audio::Capability::SetDefault
        | Audio::Capability::SetVolume | Audio::Capability::SetMute
        | Audio::Capability::MoveStream;
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
                         .canSetMute = true}};
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
                        .canSetMute = true}};
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
                         .canMove = true}};
    return snapshot;
}

} // namespace QindaQt::Tests
