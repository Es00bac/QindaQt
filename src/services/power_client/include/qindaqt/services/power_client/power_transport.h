// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

#include <QtCore/QObject>

namespace QindaQt::Power {

// One queued public mutation. Exactly the fields its OperationKind consumes;
// the service mirrors this validation, so a hostile transport cannot smuggle
// extra state through the client.
struct PowerClientRequest {
    OperationKind kind = OperationKind::SetProfile;
    QString profileId;
    QString applicationName;
    QString reason;
    Handle handle;
    quint32 value = 0;
};

// Transport implementations bind every request and signal to one exact unique
// owner. They never decide publication, retries, or operation replay policy.
// Implementations and callers share one Qt thread; every completion is emitted
// asynchronously as a bounded value and late completion is permitted.
class PowerTransport : public QObject
{
    Q_OBJECT

public:
    explicit PowerTransport(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~PowerTransport() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void fetchSnapshot(const QString &owner, quint64 requestId) = 0;
    virtual void submitOperation(const QString &owner, quint64 requestId,
                                 const PowerClientRequest &request) = 0;

Q_SIGNALS:
    void ownerChanged(const QString &owner);
    void invalidated(const QString &owner, quint64 epoch, quint64 revision);
    void snapshotReply(const QString &owner, quint64 requestId, bool transportSuccess,
                       const QindaQt::Power::Snapshot &snapshot,
                       const QString &reasonCode);
    void operationReply(const QString &owner, quint64 requestId, bool transportSuccess,
                        const QindaQt::Power::OperationResult &result,
                        const QString &reasonCode);
};

} // namespace QindaQt::Power
