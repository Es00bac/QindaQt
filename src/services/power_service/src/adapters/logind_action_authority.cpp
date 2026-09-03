// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/logind_action_authority.h>

#include <memory>
#include <QtCore/QTimer>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
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
        // Resolve the current owner so operation fencing works even when the
        // authority was already on the bus before this run started.
        resolveCurrentOwner();
        refreshAdmittedActions();
    });
    return m_generation;
}

void LogindActionAuthority::stop()
{
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

void LogindActionAuthority::resolveCurrentOwner()
{
    QDBusMessage ownerCall = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
    ownerCall.setArguments({QString::fromLatin1(kLogindServiceName)});
    auto *ownerWatch =
        new QDBusPendingCallWatcher(m_connection.asyncCall(ownerCall), this);
    connect(ownerWatch, &QDBusPendingCallWatcher::finished, this,
            [this, ownerWatch]() {
                ownerWatch->deleteLater();
                const QDBusPendingReply<QString> reply = *ownerWatch;
                if (m_running && !reply.isError() && !reply.value().isEmpty()) {
                    m_activeOwner = reply.value();
                }
            });
}

void LogindActionAuthority::refreshAdmittedActions()
{
    auto query = std::make_shared<PendingCanQuery>();
    query->outstanding = 4;
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
        QString::fromLatin1(kLogindServiceName),
        QString::fromLatin1(kLogindObjectPath),
        QString::fromLatin1(kLogindManagerInterface), canMethodName(action));
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, action, query, generation]() {
                watcher->deleteLater();
                if (!runningGeneration(generation)) {
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
    bool admitted = false;
    switch (action) {
    case SessionAction::PowerOff: admitted = m_admitted.powerOff; break;
    case SessionAction::Reboot: admitted = m_admitted.reboot; break;
    case SessionAction::Suspend: admitted = m_admitted.suspend; break;
    case SessionAction::Hibernate: admitted = m_admitted.hibernate; break;
    }
    // AGENT-GUARD: admission is checked at dispatch, not cached across the
    // call; an action whose Can* answer was not "yes" never reaches logind.
    if (!admitted) {
        finishAction(m_generation, operationId, CollaboratorStatus::Unsupported,
                     QStringLiteral("action-not-admitted"));
        return;
    }
    callExecuteAction(operationId, action);
}

void LogindActionAuthority::callExecuteAction(const quint64 operationId,
                                              const SessionAction action)
{
    const quint64 generation = m_generation;
    const QString ownerAtSubmission = m_activeOwner;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kLogindServiceName),
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
                if (!runningGeneration(generation)) {
                    return;
                }
                if (watcher->reply().type() != QDBusMessage::ReplyMessage) {
                    finishAction(generation, operationId,
                                 CollaboratorStatus::Failed,
                                 QStringLiteral("action-rejected"));
                    return;
                }
                if (!ownerAtSubmission.isEmpty()
                    && m_activeOwner != ownerAtSubmission) {
                    finishAction(generation, operationId,
                                 CollaboratorStatus::Uncertain,
                                 QStringLiteral("authority-replaced"));
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
    if (!m_running) {
        return;
    }
    if (newOwner.isEmpty()) {
        m_admitted = AdmittedActions{};
        Q_EMIT admittedActionsChanged(m_admitted);
        return;
    }
    if (!oldOwner.isEmpty() && oldOwner != newOwner) {
        m_admitted = AdmittedActions{};
        Q_EMIT admittedActionsChanged(m_admitted);
    }
    m_activeOwner = newOwner;
    refreshAdmittedActions();
}

} // namespace QindaQt::Power::Upstream
