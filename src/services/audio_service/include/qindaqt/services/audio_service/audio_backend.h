// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QObject>

namespace QindaQt::Audio
{

enum class BackendOperationStatus {
    Succeeded,
    Unsupported,
    Failed,
    Uncertain,
};

struct BackendOperationOutcome {
    BackendOperationStatus status = BackendOperationStatus::Failed;
    QString reasonCode;
    QString diagnostic;
};

// AGENT-CONTRACT: Implementations receive requests on the Qt main thread and
// publish only immutable value copies through these signals. A platform adapter
// must not expose thread-affine handles through this boundary. See ADR-0014.
class AudioBackend : public QObject
{
    Q_OBJECT

public:
    explicit AudioBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~AudioBackend() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void submit(quint64 operationId, const OperationRequest &request) = 0;

Q_SIGNALS:
    void snapshotReady(const QindaQt::Audio::Snapshot &snapshot);
    void operationFinished(quint64 operationId,
                           const QindaQt::Audio::BackendOperationOutcome &outcome);
};

} // namespace QindaQt::Audio

Q_DECLARE_METATYPE(QindaQt::Audio::BackendOperationOutcome)
