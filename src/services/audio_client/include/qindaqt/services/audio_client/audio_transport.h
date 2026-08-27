// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QObject>

namespace QindaQt::Audio
{

// Transport implementations bind every request and signal to one exact unique
// owner. They never decide publication, retries, or operation replay policy.
// Implementations and callers share one Qt thread; every completion is emitted
// asynchronously as a bounded value and late completion is permitted.
class AudioTransport : public QObject
{
    Q_OBJECT

public:
    explicit AudioTransport(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~AudioTransport() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void fetchSnapshot(const QString &owner, quint64 requestId) = 0;
    virtual void submitOperation(const QString &owner, quint64 requestId,
                                 const OperationRequest &request) = 0;

Q_SIGNALS:
    void ownerChanged(const QString &owner);
    void invalidated(const QString &owner, quint64 epoch, quint64 revision);
    void snapshotReply(const QString &owner, quint64 requestId, bool transportSuccess,
                       const QindaQt::Audio::Snapshot &snapshot,
                       const QString &reasonCode);
    void operationReply(const QString &owner, quint64 requestId, bool transportSuccess,
                        const QindaQt::Audio::OperationResult &result,
                        const QString &reasonCode);
};

} // namespace QindaQt::Audio
