// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/audio_backend.h>

#include <QtCore/QHash>
#include <QtCore/QObject>

namespace QindaQt::Audio
{

struct OperationSubmission {
    bool pending = false;
    quint64 operationId = 0;
    OperationResult immediateResult;
};

// Owns authoritative Qt-thread publication and operation lineage. The backend
// remains policy-neutral and cannot bypass stale-handle or payload validation.
// The borrowed backend shares this object's Qt thread and must outlive it;
// stop() is a publication barrier and makes every outstanding submitted
// operation uncertain. Backend run generations and snapshot lineage prevent a
// stopped or superseded adapter run from replacing authoritative state.
class AudioOperationCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit AudioOperationCoordinator(AudioBackend *backend, QObject *parent = nullptr);

    [[nodiscard]] const Snapshot &snapshot() const noexcept;
    [[nodiscard]] OperationSubmission submit(const OperationRequest &request);
    void start();
    void stop();

Q_SIGNALS:
    void snapshotChanged(const QindaQt::Audio::Snapshot &snapshot);
    void invalidated(quint64 epoch, quint64 revision);
    void operationCompleted(quint64 operationId,
                            const QindaQt::Audio::OperationResult &result);

private Q_SLOTS:
    void acceptSnapshot(quint64 generation,
                        const QindaQt::Audio::Snapshot &snapshot);
    void acceptBackendResult(quint64 generation, quint64 operationId,
                             const QindaQt::Audio::BackendOperationOutcome &outcome);

private:
    struct PendingOperation {
        OperationKind kind = OperationKind::SetDefault;
        quint64 epoch = 0;
        quint64 revision = 0;
    };

    [[nodiscard]] OperationResult immediate(const OperationRequest &request,
                                            OperationStatus status,
                                            const QString &reasonCode) const;
    [[nodiscard]] QString validateRequest(const OperationRequest &request) const;
    void makePendingUncertain(const Snapshot &observed, const QString &reasonCode);
    void publishRestartingSnapshot();

    AudioBackend *m_backend = nullptr;
    Snapshot m_snapshot;
    QHash<quint64, PendingOperation> m_pending;
    quint64 m_nextOperationId = 1;
    quint64 m_backendGeneration = 0;
    quint64 m_minimumRestartEpoch = 0;
    bool m_hasBackendSnapshot = false;
    bool m_running = false;
};

} // namespace QindaQt::Audio
