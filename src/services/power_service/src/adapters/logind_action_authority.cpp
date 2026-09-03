// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/logind_action_authority.h>

#include <memory>
#include <QtCore/QTimer>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Power::Upstream {
namespace {

constexpr char kLogindServiceName[] = "org.freedesktop.login1";
constexpr char kLogindObjectPath[] = "/org/freedesktop/login1";
constexpr char kLogindManagerInterface[] = "org.freedesktop.login1.Manager";

void applyAdmitted(const SessionAction action, const bool admitted,
                   AdmittedActions &actions)
{
    switch (action) {
    case SessionAction::PowerOff: actions.powerOff = admitted; break;
    case SessionAction::Reboot: actions.reboot = admitted; break;
    case SessionAction::Suspend: actions.suspend = admitted; break;
    case SessionAction::Hibernate: actions.hibernate = admitted; break;
    }
}

} // namespace

LogindActionAuthority::LogindActionAuthority(
    const QDBusConnection &upstreamConnection, QObject *parent)
    : QObject(parent)
    , m_connection(upstreamConnection)
{
}

LogindActionAuthority::~LogindActionAuthority()
{
    stop();
}

quint64 LogindActionAuthority::start()
{
    ++m_nextGeneration;
    if (m_nextGeneration == 0) {
        ++m_nextGeneration;
    }
    m_generation = m_nextGeneration;
    m_running = true;
    m_admitted = AdmittedActions{};
    m_activeOwner.clear();
    m_pendingAuthorizations.clear();
    m_pendingActions.clear();
    m_seenOperationIds.clear();
    if (!m_connection.isConnected()) {
        m_running = false;
        return m_generation;
    }
    if (m_watcher == nullptr) {
        m_watcher = new QDBusServiceWatcher(
            QString::fromLatin1(kLogindServiceName), m_connection,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
                &LogindActionAuthority::onLogindOwnerChanged);
    }
    QTimer::singleShot(0, this, [this]() {
        if (!runningGeneration(m_generation)) {
            return;
        }
        refreshAdmittedActions();
    });
    return m_generation;
}

void LogindActionAuthority::stop()
{
    const QList<quint64> authorizing = m_pendingAuthorizations.keys();
    m_pendingAuthorizations.clear();
    for (const quint64 operationId : authorizing) {
        finishAction(m_generation, operationId, CollaboratorStatus::Uncertain,
                     QStringLiteral("authority-stopped"));
    }
    const QList<quint64> pending = m_pendingActions.keys();
    for (const quint64 operationId : pending) {
        finishAction(m_generation, operationId, CollaboratorStatus::Uncertain,
                     QStringLiteral("authority-stopped"));
    }
    m_pendingActions.clear();
    m_running = false;
    m_admitted = AdmittedActions{};
    m_activeOwner.clear();
}

const AdmittedActions &LogindActionAuthority::admittedActions() const noexcept
{
    return m_admitted;
}

bool LogindActionAuthority::runningGeneration(const quint64 generation) const
{
    return m_running && generation != 0 && generation == m_generation;
}

QString LogindActionAuthority::canMethodName(const SessionAction action)
{
    switch (action) {
    case SessionAction::PowerOff: return QStringLiteral("CanPowerOff");
    case SessionAction::Reboot: return QStringLiteral("CanReboot");
    case SessionAction::Suspend: return QStringLiteral("CanSuspend");
    case SessionAction::Hibernate: return QStringLiteral("CanHibernate");
    }
    return QString();
}

QString LogindActionAuthority::executeMethodName(const SessionAction action)
{
    switch (action) {
    case SessionAction::PowerOff: return QStringLiteral("PowerOff");
    case SessionAction::Reboot: return QStringLiteral("Reboot");
    case SessionAction::Suspend: return QStringLiteral("Suspend");
    case SessionAction::Hibernate: return QStringLiteral("Hibernate");
    }
    return QString();
}

void LogindActionAuthority::refreshAdmittedActions()
{
    if (!m_running) {
        return;
    }
    const QDBusReply<QString> owner = m_connection.interface()->serviceOwner(
        QString::fromLatin1(kLogindServiceName));
    ++m_refreshSerial;
    if (!owner.isValid() || owner.value().isEmpty()) {
        m_activeOwner.clear();
        m_admitted = AdmittedActions{};
        Q_EMIT admittedActionsChanged(m_admitted);
        return;
    }
    m_activeOwner = owner.value();
    auto query = std::make_shared<PendingCanQuery>();
    query->outstanding = 4;
    query->serial = m_refreshSerial;
    query->owner = m_activeOwner;
    query->actions = AdmittedActions{};
    for (const SessionAction action :
         {SessionAction::PowerOff, SessionAction::Reboot, SessionAction::Suspend,
          SessionAction::Hibernate}) {
        callCanAction(action, query);
    }
}

void LogindActionAuthority::callCanAction(
    const SessionAction action, const std::shared_ptr<PendingCanQuery> &query)
{
    const quint64 generation = m_generation;
    QDBusMessage call = QDBusMessage::createMethodCall(
        query->owner,
        QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kLogindManagerInterface), canMethodName(action));
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, action, query, generation]() {
                watcher->deleteLater();
                if (!runningGeneration(generation)
                    || query->serial != m_refreshSerial
                    || query->owner != m_activeOwner) {
                    return;
                }
                const QDBusPendingReply<QString> reply = *watcher;
                bool admitted = false;
                if (!reply.isError() && reply.value() == QStringLiteral("yes")) {
                    // "challenge" is deliberately unadmitted: Power1 v1 shows
                    // no polkit UI, so an authorization requirement is honest
                    // refusal, not a prompt.
                    admitted = true;
                }
                applyAdmitted(action, admitted, query->actions);
                --query->outstanding;
                if (query->outstanding == 0) {
                    m_admitted = query->actions;
                    Q_EMIT admittedActionsChanged(m_admitted);
                }
            });
}

void LogindActionAuthority::submitAction(const quint64 operationId,
                                         const SessionAction action)
{
    if (!m_running) {
        finishAction(m_generation, operationId, CollaboratorStatus::Failed,
                     QStringLiteral("logind-unavailable"));
        return;
    }
    if (m_seenOperationIds.contains(operationId)) {
        // AGENT-GUARD: one lineage ID produces at most one terminal signal;
        // replaying a caller ID must not dispatch or double-complete it.
        return;
    }
    m_seenOperationIds.insert(operationId);
    callCanAtDispatch(operationId, action);
}

void LogindActionAuthority::callCanAtDispatch(const quint64 operationId,
                                               const SessionAction action)
{
    const QDBusReply<QString> resolved = m_connection.interface()->serviceOwner(
        QString::fromLatin1(kLogindServiceName));
    if (!resolved.isValid() || resolved.value().isEmpty()) {
        finishAction(m_generation, operationId, CollaboratorStatus::Unsupported,
                     QStringLiteral("action-not-admitted"));
        return;
    }
    const quint64 generation = m_generation;
    const QString ownerAtSubmission = resolved.value();
    m_pendingAuthorizations.insert(operationId, ownerAtSubmission);
    QDBusMessage call = QDBusMessage::createMethodCall(
        ownerAtSubmission, QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kLogindManagerInterface), canMethodName(action));
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, operationId, action,
             ownerAtSubmission]() {
                watcher->deleteLater();
                // AGENT-GUARD: A later run may legitimately reuse an operation
                // ID. Reject this callback's generation before consulting or
                // mutating the ID-keyed current-run map, or a stale Can* reply
                // can erase the restarted operation and strand it forever.
                if (!runningGeneration(generation)
                    || !m_pendingAuthorizations.contains(operationId)) {
                    return;
                }
                m_pendingAuthorizations.remove(operationId);
                const QDBusReply<QString> currentOwner =
                    m_connection.interface()->serviceOwner(
                        QString::fromLatin1(kLogindServiceName));
                if (!currentOwner.isValid()
                    || currentOwner.value() != ownerAtSubmission
                    || m_activeOwner != ownerAtSubmission) {
                    finishAction(generation, operationId,
                                 CollaboratorStatus::Uncertain,
                                 QStringLiteral("authority-replaced"));
                    return;
                }
                const QDBusPendingReply<QString> reply = *watcher;
                // AGENT-GUARD: the cached admitted set is never dispatch
                // authority. Only this exact-owner, per-operation "yes" may
                // cross into an action call; errors, "no", "na", and
                // "challenge" all terminate without an upstream mutation.
                if (reply.isError()
                    || reply.value() != QStringLiteral("yes")) {
                    finishAction(generation, operationId,
                                 CollaboratorStatus::Unsupported,
                                 QStringLiteral("action-not-admitted"));
                    return;
                }
                callExecuteAction(operationId, action, ownerAtSubmission);
            });
}

void LogindActionAuthority::callExecuteAction(const quint64 operationId,
                                              const SessionAction action,
                                              const QString &ownerAtSubmission)
{
    const quint64 generation = m_generation;
    m_pendingActions.insert(operationId, ownerAtSubmission);
    QDBusMessage call = QDBusMessage::createMethodCall(
        ownerAtSubmission,
        QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kLogindManagerInterface), executeMethodName(action));
    // One boolean argument; interactive is always false because QindaQt shows
    // no polkit UI from the power path.
    call.setArguments({false});
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, operationId, ownerAtSubmission]() {
                watcher->deleteLater();
                if (!runningGeneration(generation)
                    || !m_pendingActions.contains(operationId)) {
                    return;
                }
                m_pendingActions.remove(operationId);
                const QDBusReply<QString> currentOwner =
                    m_connection.interface()->serviceOwner(
                        QString::fromLatin1(kLogindServiceName));
                if (!currentOwner.isValid()
                    || currentOwner.value() != ownerAtSubmission
                    || m_activeOwner != ownerAtSubmission) {
                    finishAction(generation, operationId,
                                 CollaboratorStatus::Uncertain,
                                 QStringLiteral("authority-replaced"));
                    return;
                }
                if (watcher->reply().type() != QDBusMessage::ReplyMessage) {
                    finishAction(generation, operationId,
                                 CollaboratorStatus::Failed,
                                 QStringLiteral("action-rejected"));
                    return;
                }
                finishAction(generation, operationId,
                             CollaboratorStatus::Succeeded,
                             QStringLiteral("applied"));
            });
}

void LogindActionAuthority::finishAction(const quint64 generation,
                                         const quint64 operationId,
                                         const CollaboratorStatus status,
                                         const QString &reasonCode)
{
    Q_EMIT actionFinished(
        generation, operationId,
        CollaboratorOutcome{.status = status,
                            .reasonCode = reasonCode,
                            .diagnostic = {}});
}

void LogindActionAuthority::onLogindOwnerChanged(const QString &name,
                                                 const QString &oldOwner,
                                                 const QString &newOwner)
{
    Q_UNUSED(name)
    Q_UNUSED(oldOwner)
    Q_UNUSED(newOwner)
    if (!m_running) {
        return;
    }
    const QDBusReply<QString> resolved = m_connection.interface()->serviceOwner(
        QString::fromLatin1(kLogindServiceName));
    const QString currentOwner = resolved.isValid() ? resolved.value() : QString();
    if (currentOwner != m_activeOwner) {
        const QList<quint64> authorizing = m_pendingAuthorizations.keys();
        m_pendingAuthorizations.clear();
        for (const quint64 operationId : authorizing) {
            finishAction(m_generation, operationId, CollaboratorStatus::Uncertain,
                         QStringLiteral("authority-replaced"));
        }
        const QList<quint64> pending = m_pendingActions.keys();
        m_pendingActions.clear();
        for (const quint64 operationId : pending) {
            finishAction(m_generation, operationId, CollaboratorStatus::Uncertain,
                         QStringLiteral("authority-replaced"));
        }
    }
    if (currentOwner.isEmpty()) {
        ++m_refreshSerial;
        m_activeOwner.clear();
        m_admitted = AdmittedActions{};
        Q_EMIT admittedActionsChanged(m_admitted);
        return;
    }
    refreshAdmittedActions();
}

} // namespace QindaQt::Power::Upstream
