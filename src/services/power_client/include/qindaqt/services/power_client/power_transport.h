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
    // Older in-process transports remain source-compatible and fail closed
    // until they explicitly implement the public idle-lease boundary.
    virtual void queryIdleInhibitorState(const QString &owner, quint64 requestId)
    {
        Q_EMIT idleInhibitorStateReply(owner, requestId, false, 0, 0,
                                       QStringLiteral("unsupported"));
    }
    virtual void acquireIdleInhibitor(const QString &owner, quint64 requestId,
                                      const QString &application, const QString &reason,
                                      IdleInhibitorScopes scopes)
    {
        Q_UNUSED(application);
        Q_UNUSED(reason);
        Q_UNUSED(scopes);
        Q_EMIT idleInhibitorAcquireReply(owner, requestId, false, {},
                                         QStringLiteral("unsupported"));
    }
    virtual void releaseIdleInhibitor(const QString &owner, quint64 requestId,
                                      const Handle &handle)
    {
        Q_UNUSED(handle);
        Q_EMIT idleInhibitorReleaseReply(owner, requestId, false, false,
                                         QStringLiteral("unsupported"));
    }

Q_SIGNALS:
    void ownerChanged(const QString &owner);
    void invalidated(const QString &owner, quint64 epoch, quint64 revision);
    void snapshotReply(const QString &owner, quint64 requestId, bool transportSuccess,
                       const QindaQt::Power::Snapshot &snapshot,
                       const QString &reasonCode);
    void operationReply(const QString &owner, quint64 requestId, bool transportSuccess,
                        const QindaQt::Power::OperationResult &result,
                        const QString &reasonCode);
    void idleInhibitorStateReply(const QString &owner, quint64 requestId,
                                 bool transportSuccess, quint32 supportedScopes,
                                 quint32 activeScopes, const QString &reasonCode);
    void idleInhibitorAcquireReply(const QString &owner, quint64 requestId,
                                   bool transportSuccess, const QindaQt::Power::Handle &handle,
                                   const QString &reasonCode);
    void idleInhibitorReleaseReply(const QString &owner, quint64 requestId,
                                   bool transportSuccess, bool released,
                                   const QString &reasonCode);
    void idleInhibitorsChanged(const QString &owner, quint32 supportedScopes,
                               quint32 activeScopes);
};

} // namespace QindaQt::Power
