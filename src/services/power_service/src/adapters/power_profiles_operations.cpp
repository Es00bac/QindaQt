// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/power_service/adapters/power_profiles_collaborator.h>
#include "upstream_identity.h"
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusVariant>
namespace QindaQt::Power::Upstream {
namespace { constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties"; }
void PowerProfilesCollaborator::submitSetProfile(
    const quint64 operationId, const QString &profileId)
{
    if (m_activeOwner.isEmpty() || m_activeServiceName.isEmpty()) {
        finishOperation(m_generation, operationId, CollaboratorStatus::Unsupported,
                        QStringLiteral("profiles-unavailable"));
        return;
    }
    const quint64 generation = m_generation;
    const QString owner = m_activeOwner;
    QDBusMessage call = QDBusMessage::createMethodCall(
        owner, m_activeObjectPath, QString::fromLatin1(kPropertiesInterface),
        QStringLiteral("Set"));
    call.setArguments({m_activeInterfaceName, QStringLiteral("ActiveProfile"),
                       QVariant::fromValue(QDBusVariant(profileId))});
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, operationId, owner]() {
                const QDBusMessage reply = watcher->reply();
                watcher->deleteLater();
                if (!runningGeneration(generation)) {
                    return;
                }
                if (m_activeOwner != owner) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Uncertain,
                                    QStringLiteral("authority-replaced"));
                } else if (reply.type() != QDBusMessage::ReplyMessage) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Failed,
                                    QStringLiteral("profiles-rejected"));
                } else {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Succeeded,
                                    QStringLiteral("applied"));
                }
            });
}

void PowerProfilesCollaborator::submitAcquireProfileHold(
    const quint64 operationId, const QString &profileId,
    const QString &applicationName, const QString &reason)
{
    if (profileId != QStringLiteral("power-saver") && profileId != QStringLiteral("performance")) {
        finishOperation(m_generation, operationId, CollaboratorStatus::Unsupported,
                        QStringLiteral("profile-not-holdable"));
        return;
    }
    if (m_activeOwner.isEmpty() || m_activeServiceName.isEmpty()) {
        finishOperation(m_generation, operationId, CollaboratorStatus::Unsupported,
                        QStringLiteral("profiles-unavailable"));
        return;
    }
    const quint64 generation = m_generation;
    const QString owner = m_activeOwner;
    QDBusMessage call = QDBusMessage::createMethodCall(
        owner, m_activeObjectPath, m_activeInterfaceName,
        QStringLiteral("HoldProfile"));
    call.setArguments({profileId, reason, applicationName});
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(call, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, operationId, profileId, applicationName,
             reason, owner]() {
                const QDBusMessage reply = watcher->reply();
                watcher->deleteLater();
                if (!runningGeneration(generation)) {
                    return;
                }
                if (m_activeOwner != owner) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Uncertain,
                                    QStringLiteral("authority-replaced"));
                    return;
                }
                if (reply.type() != QDBusMessage::ReplyMessage
                    || reply.arguments().size() != 1
                    || reply.arguments().constFirst().metaType()
                        != QMetaType::fromType<uint>()) {
                    const bool uncertain = reply.type() == QDBusMessage::ReplyMessage
                        || reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.NoReply")
                        || reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.Timeout")
                        || reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.Disconnected");
                    finishOperation(generation, operationId,
                                    uncertain ? CollaboratorStatus::Uncertain : CollaboratorStatus::Failed,
                                    uncertain ? QStringLiteral("profiles-uncertain")
                                              : QStringLiteral("profiles-rejected"));
                    return;
                }
                const quint32 cookie = reply.arguments().constFirst().toUInt();
                const QString opaqueId = deriveOpaqueId(
                    QStringLiteral("profile-hold"),
                    profileId + QLatin1Char('|') + applicationName
                        + QLatin1Char('|') + reason);
                m_acquiredHolds.insert(opaqueId, cookie);
                refreshFacts(generation);
                finishOperation(generation, operationId,
                                CollaboratorStatus::Succeeded,
                                QStringLiteral("applied"));
            });
}

void PowerProfilesCollaborator::submitReleaseProfileHold(
    const quint64 operationId, const Handle &hold)
{
    const auto it = m_acquiredHolds.constFind(hold.opaqueId);
    if (it == m_acquiredHolds.constEnd()) {
        finishOperation(m_generation, operationId, CollaboratorStatus::Unsupported,
                        QStringLiteral("hold-not-releasable"));
        return;
    }
    callRelease(it.value(), m_generation, operationId, m_activeOwner);
}

void PowerProfilesCollaborator::callRelease(
    const quint32 cookie, const quint64 generation, const quint64 operationId,
    const QString &ownerAtSubmission)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        ownerAtSubmission, m_activeObjectPath, m_activeInterfaceName,
        QStringLiteral("ReleaseProfile"));
    call.setArguments({cookie});
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(call, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, operationId, cookie, ownerAtSubmission]() {
                const QDBusMessage reply = watcher->reply();
                watcher->deleteLater();
                if (!runningGeneration(generation)) {
                    return;
                }
                if (m_activeOwner != ownerAtSubmission) {
                    finishOperation(generation, operationId,
                                    CollaboratorStatus::Uncertain,
                                    QStringLiteral("authority-replaced"));
                    return;
                }
                if (reply.type() != QDBusMessage::ReplyMessage) {
                    const bool uncertain = reply.type() == QDBusMessage::ReplyMessage
                        || reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.NoReply")
                        || reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.Timeout")
                        || reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.Disconnected");
                    finishOperation(generation, operationId,
                                    uncertain ? CollaboratorStatus::Uncertain : CollaboratorStatus::Failed,
                                    uncertain ? QStringLiteral("profiles-uncertain")
                                              : QStringLiteral("profiles-rejected"));
                    return;
                }
                for (auto it = m_acquiredHolds.begin();
                     it != m_acquiredHolds.end(); ++it) {
                    if (it.value() == cookie) {
                        m_acquiredHolds.erase(it);
                        break;
                    }
                }
                refreshFacts(generation);
                finishOperation(generation, operationId,
                                CollaboratorStatus::Succeeded,
                                QStringLiteral("applied"));
            });
}

} // namespace QindaQt::Power::Upstream
