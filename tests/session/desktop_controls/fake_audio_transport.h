// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/audio_client/audio_transport.h>

#include <QList>
#include <QString>

namespace QindaQt::Session::DesktopControls::Tests {

using QindaQt::Audio::OperationRequest;
using QindaQt::Audio::OperationResult;
using QindaQt::Audio::Snapshot;

// Drives the real AudioClient with scripted owner/snapshot/operation replies.
// Each fetch is answered with the queued snapshot; each operation is answered
// with the queued result. Nothing contacts a bus.
class FakeAudioTransport final : public Audio::AudioTransport {
    Q_OBJECT

public:
    using AudioTransport::AudioTransport;

    void start() override {}
    void stop() override {}

    void fetchSnapshot(const QString &owner, quint64 requestId) override
    {
        m_fetchOwners.append(owner);
        if (m_snapshots.isEmpty()) {
            return;
        }
        const Snapshot snapshot = m_snapshots.takeFirst();
        QMetaObject::invokeMethod(
            this,
            [this, owner, requestId, snapshot] {
                Q_EMIT snapshotReply(owner, requestId, true, snapshot, QString{});
            },
            Qt::QueuedConnection);
    }

    void submitOperation(const QString &owner, quint64 requestId,
                         const Audio::OperationRequest &request) override
    {
        m_operations.append(request);
        const Audio::OperationResult result = m_results.isEmpty()
            ? Audio::OperationResult{request.kind, Audio::OperationStatus::Succeeded,
                                     request.primary.epoch, 1, request.primary.epoch, 2,
                                     QString{}, QString{}}
            : m_results.takeFirst();
        QMetaObject::invokeMethod(
            this,
            [this, owner, requestId, result] {
                Q_EMIT operationReply(owner, requestId, true, result, QString{});
            },
            Qt::QueuedConnection);
    }

    void announceOwner(const QString &owner)
    {
        QMetaObject::invokeMethod(
            this, [this, owner] { Q_EMIT ownerChanged(owner); },
            Qt::QueuedConnection);
    }

    void queueSnapshot(const Snapshot &snapshot) { m_snapshots.append(snapshot); }
    void queueResult(const Audio::OperationResult &result) { m_results.append(result); }

    void reset()
    {
        m_snapshots.clear();
        m_results.clear();
        m_operations.clear();
        m_fetchOwners.clear();
    }

    [[nodiscard]] const QList<Audio::OperationRequest> &operations() const noexcept
    {
        return m_operations;
    }
    [[nodiscard]] const QStringList &fetchOwners() const noexcept { return m_fetchOwners; }

private:
    QList<Snapshot> m_snapshots;
    QList<Audio::OperationResult> m_results;
    QList<Audio::OperationRequest> m_operations;
    QStringList m_fetchOwners;
};

} // namespace QindaQt::Session::DesktopControls::Tests
