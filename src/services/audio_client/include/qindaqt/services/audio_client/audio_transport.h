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
    // The meter stream (Audio1 `Levels`) is the one high-rate signal: twenty
    // frames a second, decoded and queued for every subscriber whether or not
    // a meter is on screen. It is therefore subscribed only while the client
    // reports at least one consumer (AudioClient::retainLevels, ADR-0394).
    // The request is a standing want, not a one-shot: an implementation applies
    // it to the current owner at once and to every later owner, and keeps it
    // across stop()/start(). `Changed` stays subscribed regardless. The default
    // does nothing, for transports that carry no live meters.
    virtual void setLevelsSubscribed(bool subscribed) { Q_UNUSED(subscribed) }
    virtual void fetchSnapshot(const QString &owner, quint64 requestId) = 0;
    virtual void submitOperation(const QString &owner, quint64 requestId,
                                 const OperationRequest &request) = 0;

Q_SIGNALS:
    void ownerChanged(const QString &owner);
    void invalidated(const QString &owner, quint64 epoch, quint64 revision);
    void snapshotReply(const QString &owner, quint64 requestId, bool transportSuccess,
                       const QindaQt::Audio::Snapshot &snapshot,
                       const QString &reasonCode);
    // Meter readings from the bound owner (ADR-0174). Carries no lineage: a
    // batch is applied onto whatever snapshot the client already holds, and a
    // dropped batch is one skipped frame rather than a state divergence.
    void levelsReceived(const QString &owner,
                        const QList<QindaQt::Audio::LevelReading> &levels);
    void operationReply(const QString &owner, quint64 requestId, bool transportSuccess,
                        const QindaQt::Audio::OperationResult &result,
                        const QString &reasonCode);
};

} // namespace QindaQt::Audio
