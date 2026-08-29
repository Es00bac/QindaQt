// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_service_object_p.h"

#include <QtDBus/QDBusConnection>

namespace QindaQt::Power {

PowerServiceObject::PowerServiceObject(PowerServiceCoordinator *coordinator,
                                       const QDBusConnection &connection,
                                       QObject *parent)
    : QObject(parent)
    , m_coordinator(coordinator)
    , m_connection(connection)
{
    Q_ASSERT(m_coordinator != nullptr);
    connect(m_coordinator, &PowerServiceCoordinator::invalidated, this,
            &PowerServiceObject::Changed);
    connect(m_coordinator, &PowerServiceCoordinator::operationCompleted, this,
            &PowerServiceObject::finishOperation);
}

Snapshot PowerServiceObject::GetSnapshot() const
{
    return m_coordinator->snapshot();
}

void PowerServiceObject::SetProfile(const QString &profileId)
{
    beginOperation({.kind = OperationKind::SetProfile,
                    .profileId = profileId,
                    .applicationName = {},
                    .reason = {},
                    .handle = {},
                    .value = 0});
}

void PowerServiceObject::AcquireProfileHold(const QString &profileId,
                                            const QString &applicationName,
                                            const QString &reason)
{
    beginOperation({.kind = OperationKind::AcquireProfileHold,
                    .profileId = profileId,
                    .applicationName = applicationName,
                    .reason = reason,
                    .handle = {},
                    .value = 0});
}

void PowerServiceObject::ReleaseProfileHold(const Handle &hold)
{
    beginOperation({.kind = OperationKind::ReleaseProfileHold,
                    .profileId = {},
                    .applicationName = {},
                    .reason = {},
                    .handle = hold,
                    .value = 0});
}

void PowerServiceObject::SetKeyboardBrightness(const Handle &device,
                                               const quint32 value)
{
    beginOperation({.kind = OperationKind::SetKeyboardBrightness,
                    .profileId = {},
                    .applicationName = {},
                    .reason = {},
                    .handle = device,
                    .value = value});
}

void PowerServiceObject::beginOperation(const PowerServiceRequest &request)
{
    if (!calledFromDBus()) {
        return;
    }

    const QDBusMessage call = message();
    setDelayedReply(true);
    const OperationSubmission submission = m_coordinator->submit(request);
    if (!submission.pending) {
        m_connection.send(
            call.createReply(QVariant::fromValue(submission.immediateResult)));
        return;
    }

    // AGENT-GUARD: The coordinator caps all pending operations before this
    // insertion. Keeping exactly one original call per operation prevents a
    // timeout or authority replacement from being accidentally replayed.
    m_pendingReplies.insert(submission.operationId, call);
}

void PowerServiceObject::finishOperation(const quint64 operationId,
                                         const OperationResult &result)
{
    const auto it = m_pendingReplies.find(operationId);
    if (it == m_pendingReplies.end()) {
        return;
    }
    const QDBusMessage call = it.value();
    m_pendingReplies.erase(it);
    // QDBusContext is valid only during the original method invocation. The
    // retained message and explicitly owned connection are the complete async
    // reply capability; consulting QDBusContext here would dereference expired
    // call-local state.
    m_connection.send(call.createReply(QVariant::fromValue(result)));
}

} // namespace QindaQt::Power
