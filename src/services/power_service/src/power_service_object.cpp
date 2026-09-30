// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_service_object_p.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Power {

PowerServiceObject::PowerServiceObject(PowerServiceCoordinator *coordinator,
                                       const QDBusConnection &connection,
                                       QObject *parent)
    : QObject(parent)
    , m_coordinator(coordinator)
    , m_connection(connection)
{
    Q_ASSERT(m_coordinator != nullptr);
    m_ownerWatcher = new QDBusServiceWatcher(
        QString{}, m_connection, QDBusServiceWatcher::WatchForUnregistration,
        this);
    connect(m_ownerWatcher, &QDBusServiceWatcher::serviceUnregistered, this,
            [this](const QString &owner) {
                m_watchedOwners.remove(owner);
                m_idleInhibitors.ownerVanished(owner);
                Q_EMIT IdleInhibitorsChanged(
                    static_cast<quint32>(m_idleInhibitors.consumedScopes().toInt()),
                    activeIdleInhibitorScopes());
            });
    connect(m_coordinator, &PowerServiceCoordinator::invalidated, this,
            &PowerServiceObject::Changed);
    connect(m_coordinator, &PowerServiceCoordinator::invalidated, this,
            [this](const quint64 epoch, const quint64) {
                const bool epochChanged = m_idleInhibitorEpoch != epoch;
                synchronizeIdleInhibitorEpoch(epoch);
                if (epochChanged) {
                    Q_EMIT IdleInhibitorsChanged(
                        static_cast<quint32>(m_idleInhibitors.consumedScopes().toInt()),
                        activeIdleInhibitorScopes());
                }
            });
    connect(m_coordinator, &PowerServiceCoordinator::operationCompleted, this,
            &PowerServiceObject::finishOperation);
}

Snapshot PowerServiceObject::GetSnapshot() const
{
    const Snapshot current = m_coordinator->snapshot();
    synchronizeIdleInhibitorEpoch(current.epoch);
    return current;
}

quint32 PowerServiceObject::GetIdleInhibitorCapabilities() const
{
    const Snapshot current = m_coordinator->snapshot();
    synchronizeIdleInhibitorEpoch(current.epoch);
    return static_cast<quint32>(m_idleInhibitors.consumedScopes().toInt());
}

quint32 PowerServiceObject::GetActiveIdleInhibitorScopes() const
{
    const Snapshot current = m_coordinator->snapshot();
    synchronizeIdleInhibitorEpoch(current.epoch);
    return activeIdleInhibitorScopes();
}

QindaQt::Power::Handle PowerServiceObject::AcquireIdleInhibitor(
    const QString &application, const QString &reason, const quint32 scopes)
{
    if (!calledFromDBus()) {
        return {};
    }
    const QDBusMessage call = message();
    synchronizeIdleInhibitorEpoch(m_coordinator->snapshot().epoch);
    const auto result = m_idleInhibitors.acquire(
        call.service(), application, reason, IdleInhibitorScopes::fromInt(scopes));
    if (result.status != IdleInhibitorAcquireStatus::Accepted) {
        setDelayedReply(true);
        const QString name =
            result.status == IdleInhibitorAcquireStatus::Unsupported
                ? QStringLiteral("org.qindaqt.Power1.Error.Unsupported")
            : result.status == IdleInhibitorAcquireStatus::Capacity
                ? QStringLiteral("org.qindaqt.Power1.Error.Busy")
                : QStringLiteral("org.qindaqt.Power1.Error.Invalid");
        m_connection.send(call.createErrorReply(name, result.reasonCode));
        return {};
    }

    const QString owner = call.service();
    if (!m_watchedOwners.contains(owner)) {
        m_watchedOwners.insert(owner);
        m_ownerWatcher->addWatchedService(owner);
    }
    Q_EMIT IdleInhibitorsChanged(
        static_cast<quint32>(m_idleInhibitors.consumedScopes().toInt()),
        activeIdleInhibitorScopes());
    return result.handle;
}

bool PowerServiceObject::ReleaseIdleInhibitor(
    const QindaQt::Power::Handle &handle)
{
    if (!calledFromDBus()) {
        return false;
    }
    const QDBusMessage call = message();
    synchronizeIdleInhibitorEpoch(m_coordinator->snapshot().epoch);
    const bool released = m_idleInhibitors.release(call.service(), handle);
    if (released) {
        if (m_idleInhibitors.leaseCountForOwner(call.service()) == 0) {
            m_watchedOwners.remove(call.service());
            m_ownerWatcher->removeWatchedService(call.service());
        }
        Q_EMIT IdleInhibitorsChanged(
            static_cast<quint32>(m_idleInhibitors.consumedScopes().toInt()),
            activeIdleInhibitorScopes());
    }
    return released;
}

void PowerServiceObject::synchronizeIdleInhibitorEpoch(
    const quint64 epoch) const
{
    if (epoch == m_idleInhibitorEpoch) {
        return;
    }
    for (const QString &owner : m_watchedOwners) {
        m_ownerWatcher->removeWatchedService(owner);
    }
    m_watchedOwners.clear();
    m_idleInhibitors.setEpoch(epoch);
    m_idleInhibitorEpoch = epoch;
}

quint32 PowerServiceObject::activeIdleInhibitorScopes() const
{
    quint32 active = 0;
    if (m_idleInhibitors.isInhibited(IdleInhibitorScope::AutomaticLock)) {
        active |= static_cast<quint32>(IdleInhibitorScope::AutomaticLock);
    }
    if (m_idleInhibitors.isInhibited(IdleInhibitorScope::DisplayOff)) {
        active |= static_cast<quint32>(IdleInhibitorScope::DisplayOff);
    }
    if (m_idleInhibitors.isInhibited(IdleInhibitorScope::IdleSuspend)) {
        active |= static_cast<quint32>(IdleInhibitorScope::IdleSuspend);
    }
    return active;
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

void PowerServiceObject::SetInternalBrightness(const Handle &device,
                                               const quint32 value)
{
    beginOperation({.kind = OperationKind::SetInternalBrightness,
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
