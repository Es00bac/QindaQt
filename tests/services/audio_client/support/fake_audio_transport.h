// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_client/audio_transport.h>

namespace QindaQt::Tests
{

class FakeAudioTransport final : public Audio::AudioTransport
{
public:
    using AudioTransport::AudioTransport;

    struct Fetch {
        QString owner;
        quint64 requestId = 0;
    };
    struct Operation {
        QString owner;
        quint64 requestId = 0;
        Audio::OperationRequest request;
    };

    void start() override { ++startCalls; }
    void stop() override { ++stopCalls; }
    void fetchSnapshot(const QString &owner, const quint64 requestId) override
    {
        fetches.push_back({owner, requestId});
    }
    void submitOperation(const QString &owner, const quint64 requestId,
                         const Audio::OperationRequest &request) override
    {
        operations.push_back({owner, requestId, request});
    }

    void announceOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
    void invalidate(const QString &owner, const quint64 epoch, const quint64 revision)
    {
        Q_EMIT invalidated(owner, epoch, revision);
    }
    void reply(const Fetch &fetch, const Audio::Snapshot &snapshot)
    {
        Q_EMIT snapshotReply(fetch.owner, fetch.requestId, true, snapshot, {});
    }
    void fail(const Fetch &fetch, const QString &reasonCode)
    {
        Q_EMIT snapshotReply(fetch.owner, fetch.requestId, false, {}, reasonCode);
    }
    void finish(const Operation &operation, const Audio::OperationResult &result)
    {
        Q_EMIT operationReply(operation.owner, operation.requestId, true, result, {});
    }
    void fail(const Operation &operation, const QString &reasonCode)
    {
        Q_EMIT operationReply(operation.owner, operation.requestId, false, {}, reasonCode);
    }

    QList<Fetch> fetches;
    QList<Operation> operations;
    int startCalls = 0;
    int stopCalls = 0;
};

inline Audio::Snapshot clientSnapshot(const quint64 epoch = 11,
                                      const quint64 revision = 2)
{
    Audio::Snapshot snapshot;
    snapshot.epoch = epoch;
    snapshot.revision = revision;
    snapshot.availability = Audio::Availability::Ready;
    snapshot.capabilities = Audio::Capability::SetDefault
        | Audio::Capability::SetVolume | Audio::Capability::SetMute
        | Audio::Capability::MoveStream;
    snapshot.defaultOutput = {.epoch = epoch, .serial = 10};
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
    return snapshot;
}

inline Audio::OperationResult successfulResult(const FakeAudioTransport::Operation &op,
                                                const quint64 observedRevision)
{
    return {.kind = op.request.kind,
            .status = Audio::OperationStatus::Succeeded,
            .initiatingEpoch = op.request.primary.epoch,
            .initiatingRevision = 2,
            .observedEpoch = op.request.primary.epoch,
            .observedRevision = observedRevision,
            .reasonCode = QStringLiteral("ok"),
            .diagnostic = {},
            .wireValid = true};
}

} // namespace QindaQt::Tests
