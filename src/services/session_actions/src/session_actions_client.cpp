// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/session_actions/session_actions_client.h>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusServiceWatcher>

#include <memory>
#include <utility>

namespace QindaQt::Services::SessionActions {
namespace {

constexpr auto SessionService = "org.qindaqt.Session1";
constexpr auto SessionPath = "/org/qindaqt/Session1";
constexpr auto SessionInterface = "org.qindaqt.Session1";
constexpr auto ScreenSaverService = "org.freedesktop.ScreenSaver";
constexpr auto ScreenSaverPath = "/ScreenSaver";
constexpr auto ScreenSaverInterface = "org.freedesktop.ScreenSaver";
constexpr auto LogindService = "org.freedesktop.login1";
constexpr auto LogindPath = "/org/freedesktop/login1";
constexpr auto LogindInterface = "org.freedesktop.login1.Manager";

QString serviceOwner(const QDBusConnection &connection, const char *service)
{
    if (!connection.isConnected() || connection.interface() == nullptr) {
        return {};
    }
    const QDBusReply<QString> reply =
        connection.interface()->serviceOwner(QString::fromLatin1(service));
    return reply.isValid() ? reply.value() : QString{};
}

QString canMethod(SessionAction action)
{
    switch (action) {
    case SessionAction::Suspend: return QStringLiteral("CanSuspend");
    case SessionAction::Reboot: return QStringLiteral("CanReboot");
    case SessionAction::PowerOff: return QStringLiteral("CanPowerOff");
    case SessionAction::Lock:
    case SessionAction::Logout: break;
    }
    return {};
}

QString actionMethod(SessionAction action)
{
    switch (action) {
    case SessionAction::Lock: return QStringLiteral("Lock");
    case SessionAction::Logout: return QStringLiteral("Logout");
    case SessionAction::Suspend: return QStringLiteral("Suspend");
    case SessionAction::Reboot: return QStringLiteral("Reboot");
    case SessionAction::PowerOff: return QStringLiteral("PowerOff");
    }
    return {};
}

QString unavailableText(SessionAction action)
{
    switch (action) {
    case SessionAction::Lock: return SessionActionsClient::tr("Screen locking is unavailable.");
    case SessionAction::Logout: return SessionActionsClient::tr("Logging out is unavailable.");
    case SessionAction::Suspend: return SessionActionsClient::tr("Suspend is unavailable.");
    case SessionAction::Reboot: return SessionActionsClient::tr("Restart is unavailable.");
    case SessionAction::PowerOff: return SessionActionsClient::tr("Shut down is unavailable.");
    }
    return SessionActionsClient::tr("The session action is unavailable.");
}

} // namespace

struct SessionActionsClient::RefreshQuery final {
    quint64 serial = 0;
    quint64 screenSaverEpoch = 0;
    quint64 sessionEpoch = 0;
    quint64 logindEpoch = 0;
    int outstanding = 0;
    SessionActionAvailability availability;
};

SessionActionsClient::SessionActionsClient(QDBusConnection sessionBus,
                                           QDBusConnection systemBus,
                                           QObject *parent)
    : QObject(parent)
    , m_sessionBus(std::move(sessionBus))
    , m_systemBus(std::move(systemBus))
{
    m_refreshDebounce.setSingleShot(true);
    m_refreshDebounce.setInterval(0);
    m_refreshDeadline.setSingleShot(true);
    m_refreshDeadline.setInterval(RefreshTimeoutMilliseconds);
    m_actionDeadline.setSingleShot(true);
    m_actionDeadline.setInterval(ActionTimeoutMilliseconds);
    connect(&m_refreshDebounce, &QTimer::timeout, this,
            &SessionActionsClient::refreshAvailability);
    connect(&m_actionDeadline, &QTimer::timeout, this, [this] {
        if (!m_pending) {
            return;
        }
        completePending(m_pending->mutationDispatched ? ActionStatus::Uncertain
                                                      : ActionStatus::Unavailable,
                        QStringLiteral("request-timeout"));
    });
}

SessionActionsClient::~SessionActionsClient()
{
    stop();
}

void SessionActionsClient::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    auto installWatcher = [this](QDBusServiceWatcher *&slot,
                                 const char *service,
                                 const QDBusConnection &connection,
                                 SessionAction authority) {
        slot = new QDBusServiceWatcher(QString::fromLatin1(service), connection,
                                       QDBusServiceWatcher::WatchForOwnerChange,
                                       this);
        connect(slot, &QDBusServiceWatcher::serviceOwnerChanged, this,
                [this, authority](const QString &, const QString &, const QString &) {
                    advanceAuthorityEpoch(authority);
                    ++m_refreshSerial;
                    publishAvailability({});
                    scheduleRefresh();
                });
    };
    if (m_sessionBus.isConnected()) {
        installWatcher(m_sessionWatcher, SessionService, m_sessionBus,
                       SessionAction::Logout);
        installWatcher(m_screenSaverWatcher, ScreenSaverService, m_sessionBus,
                       SessionAction::Lock);
    }
    if (m_systemBus.isConnected()) {
        installWatcher(m_logindWatcher, LogindService, m_systemBus,
                       SessionAction::Suspend);
    }
    scheduleRefresh();
}

void SessionActionsClient::stop()
{
    if (!m_running && !m_pending) {
        return;
    }
    m_running = false;
    ++m_refreshSerial;
    m_refreshDebounce.stop();
    m_refreshDeadline.stop();
    if (m_pending) {
        completePending(m_pending->mutationDispatched ? ActionStatus::Uncertain
                                                      : ActionStatus::Unavailable,
                        QStringLiteral("client-stopped"));
    }
    delete m_sessionWatcher;
    delete m_screenSaverWatcher;
    delete m_logindWatcher;
    m_sessionWatcher = nullptr;
    m_screenSaverWatcher = nullptr;
    m_logindWatcher = nullptr;
    publishAvailability({});
}

void SessionActionsClient::refresh()
{
    if (m_running) {
        scheduleRefresh();
    }
}

void SessionActionsClient::scheduleRefresh()
{
    if (m_running && !m_refreshDebounce.isActive()) {
        m_refreshDebounce.start();
    }
}

void SessionActionsClient::refreshAvailability()
{
    if (!m_running) {
        return;
    }
    const quint64 serial = ++m_refreshSerial;
    auto query = std::make_shared<RefreshQuery>();
    query->serial = serial;

    const QString screenSaverOwner = serviceOwner(m_sessionBus, ScreenSaverService);
    const QString sessionOwner = serviceOwner(m_sessionBus, SessionService);
    const QString logindOwner = serviceOwner(m_systemBus, LogindService);
    query->screenSaverEpoch = m_screenSaverEpoch;
    query->sessionEpoch = m_sessionEpoch;
    query->logindEpoch = m_logindEpoch;
    query->outstanding = (screenSaverOwner.isEmpty() ? 0 : 1)
        + (sessionOwner.isEmpty() ? 0 : 1)
        + (logindOwner.isEmpty() ? 0 : 3);
    if (query->outstanding == 0) {
        publishAvailability(query->availability);
        return;
    }

    m_refreshDeadline.start();
    QObject::disconnect(&m_refreshDeadline, nullptr, this, nullptr);
    connect(&m_refreshDeadline, &QTimer::timeout, this,
            [this, query] {
                if (m_running && query->serial == m_refreshSerial) {
                    ++m_refreshSerial;
                    publishAvailability(query->availability);
                }
            }, Qt::SingleShotConnection);

    if (!screenSaverOwner.isEmpty()) {
        QDBusMessage call = QDBusMessage::createMethodCall(
            screenSaverOwner, QString::fromLatin1(ScreenSaverPath),
            QString::fromLatin1(ScreenSaverInterface), QStringLiteral("GetActive"));
        auto *watcher = new QDBusPendingCallWatcher(m_sessionBus.asyncCall(call), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher, query, screenSaverOwner] {
                    const QDBusPendingReply<bool> reply = *watcher;
                    watcher->deleteLater();
                    if (query->serial != m_refreshSerial || !m_running) {
                        return;
                    }
                    // AGENT-GUARD: Name ownership alone does not prove that the
                    // standard /ScreenSaver lock interface exists.
                    query->availability.lock = !reply.isError()
                        && serviceOwner(m_sessionBus, ScreenSaverService) == screenSaverOwner
                        && m_screenSaverEpoch == query->screenSaverEpoch;
                    completeRefresh(query);
                });
    }

    if (!sessionOwner.isEmpty()) {
        QDBusMessage call = QDBusMessage::createMethodCall(
            sessionOwner, QString::fromLatin1(SessionPath),
            QString::fromLatin1(SessionInterface), QStringLiteral("CanLogout"));
        auto *watcher = new QDBusPendingCallWatcher(m_sessionBus.asyncCall(call), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher, query, sessionOwner] {
                    const QDBusPendingReply<bool> reply = *watcher;
                    watcher->deleteLater();
                    if (query->serial != m_refreshSerial || !m_running) {
                        return;
                    }
                    query->availability.logout = !reply.isError() && reply.value()
                        && serviceOwner(m_sessionBus, SessionService) == sessionOwner
                        && m_sessionEpoch == query->sessionEpoch;
                    completeRefresh(query);
                });
    }

    for (const SessionAction action : {SessionAction::Suspend,
                                       SessionAction::Reboot,
                                       SessionAction::PowerOff}) {
        if (logindOwner.isEmpty()) {
            break;
        }
        QDBusMessage call = QDBusMessage::createMethodCall(
            logindOwner, QString::fromLatin1(LogindPath),
            QString::fromLatin1(LogindInterface), canMethod(action));
        auto *watcher = new QDBusPendingCallWatcher(m_systemBus.asyncCall(call), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher, query, logindOwner, action] {
                    const QDBusPendingReply<QString> reply = *watcher;
                    watcher->deleteLater();
                    if (query->serial != m_refreshSerial || !m_running) {
                        return;
                    }
                    const bool admitted = !reply.isError()
                        && reply.value() == QStringLiteral("yes")
                        && serviceOwner(m_systemBus, LogindService) == logindOwner
                        && m_logindEpoch == query->logindEpoch;
                    switch (action) {
                    case SessionAction::Suspend: query->availability.suspend = admitted; break;
                    case SessionAction::Reboot: query->availability.reboot = admitted; break;
                    case SessionAction::PowerOff: query->availability.powerOff = admitted; break;
                    case SessionAction::Lock:
                    case SessionAction::Logout: break;
                    }
                    completeRefresh(query);
                });
    }
}

void SessionActionsClient::completeRefresh(const std::shared_ptr<RefreshQuery> &query)
{
    --query->outstanding;
    if (query->outstanding == 0 && query->serial == m_refreshSerial) {
        m_refreshDeadline.stop();
        publishAvailability(query->availability);
    }
}

QString SessionActionsClient::currentOwner(SessionAction action) const
{
    if (action == SessionAction::Lock) {
        return serviceOwner(m_sessionBus, ScreenSaverService);
    }
    if (action == SessionAction::Logout) {
        return serviceOwner(m_sessionBus, SessionService);
    }
    return serviceOwner(m_systemBus, LogindService);
}

quint64 SessionActionsClient::authorityEpoch(SessionAction action) const noexcept
{
    if (action == SessionAction::Lock) {
        return m_screenSaverEpoch;
    }
    if (action == SessionAction::Logout) {
        return m_sessionEpoch;
    }
    return m_logindEpoch;
}

bool SessionActionsClient::authorityMatches(const PendingAction &request) const
{
    return authorityEpoch(request.action) == request.authorityEpoch
        && currentOwner(request.action) == request.owner;
}

void SessionActionsClient::advanceAuthorityEpoch(SessionAction action)
{
    if (action == SessionAction::Lock) {
        ++m_screenSaverEpoch;
    } else if (action == SessionAction::Logout) {
        ++m_sessionEpoch;
    } else {
        ++m_logindEpoch;
    }
}

bool SessionActionsClient::requestAction(SessionAction action)
{
    if (!m_running || !cachedAvailable(action)) {
        publishFeedback(unavailableText(action));
        return false;
    }
    if (m_pending) {
        publishFeedback(tr("Another session action is already in progress."));
        return false;
    }
    const QString owner = currentOwner(action);
    if (owner.isEmpty()) {
        publishAvailability({});
        publishFeedback(unavailableText(action));
        scheduleRefresh();
        return false;
    }
    ++m_nextRequestId;
    if (m_nextRequestId == 0) {
        publishFeedback(tr("The session action request limit was reached."));
        return false;
    }
    m_pending = PendingAction{m_nextRequestId, action, owner,
                              authorityEpoch(action), false};
    m_actionDeadline.start();
    publishFeedback({});
    Q_EMIT pendingChanged();
    if (action == SessionAction::Lock) {
        authorizeLock();
    } else if (action == SessionAction::Logout) {
        authorizeLogout();
    } else {
        authorizeLogind();
    }
    return true;
}

void SessionActionsClient::authorizeLock()
{
    const PendingAction request = *m_pending;
    QDBusMessage call = QDBusMessage::createMethodCall(
        request.owner, QString::fromLatin1(ScreenSaverPath),
        QString::fromLatin1(ScreenSaverInterface), QStringLiteral("GetActive"));
    auto *watcher = new QDBusPendingCallWatcher(m_sessionBus.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, request] {
                const QDBusPendingReply<bool> reply = *watcher;
                watcher->deleteLater();
                if (!m_pending || m_pending->requestId != request.requestId) {
                    return;
                }
                if (!authorityMatches(request)) {
                    completePending(ActionStatus::Unavailable,
                                    QStringLiteral("authority-replaced"));
                } else if (reply.isError()) {
                    completePending(ActionStatus::Unavailable,
                                    QStringLiteral("lock-interface-unavailable"));
                } else {
                    dispatchMutation();
                }
            });
}

void SessionActionsClient::authorizeLogout()
{
    const PendingAction request = *m_pending;
    QDBusMessage call = QDBusMessage::createMethodCall(
        request.owner, QString::fromLatin1(SessionPath),
        QString::fromLatin1(SessionInterface), QStringLiteral("CanLogout"));
    auto *watcher = new QDBusPendingCallWatcher(m_sessionBus.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, request] {
                const QDBusPendingReply<bool> reply = *watcher;
                watcher->deleteLater();
                if (!m_pending || m_pending->requestId != request.requestId) {
                    return;
                }
                if (!authorityMatches(request)) {
                    completePending(ActionStatus::Unavailable,
                                    QStringLiteral("authority-replaced"));
                } else if (reply.isError() || !reply.value()) {
                    completePending(ActionStatus::Rejected,
                                    QStringLiteral("action-not-admitted"));
                } else {
                    dispatchMutation();
                }
            });
}

void SessionActionsClient::authorizeLogind()
{
    const PendingAction request = *m_pending;
    QDBusMessage call = QDBusMessage::createMethodCall(
        request.owner, QString::fromLatin1(LogindPath),
        QString::fromLatin1(LogindInterface), canMethod(request.action));
    auto *watcher = new QDBusPendingCallWatcher(m_systemBus.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, request] {
                const QDBusPendingReply<QString> reply = *watcher;
                watcher->deleteLater();
                if (!m_pending || m_pending->requestId != request.requestId) {
                    return;
                }
                if (!authorityMatches(request)) {
                    completePending(ActionStatus::Unavailable,
                                    QStringLiteral("authority-replaced"));
                } else if (reply.isError()
                           || reply.value() != QStringLiteral("yes")) {
                    completePending(ActionStatus::Rejected,
                                    QStringLiteral("action-not-admitted"));
                } else {
                    dispatchMutation();
                }
            });
}

void SessionActionsClient::dispatchMutation()
{
    if (!m_pending) {
        return;
    }
    const PendingAction request = *m_pending;
    if (!authorityMatches(request)) {
        completePending(ActionStatus::Unavailable,
                        QStringLiteral("authority-replaced"));
        return;
    }
    QDBusMessage call;
    QDBusConnection *connection = nullptr;
    if (request.action == SessionAction::Lock) {
        connection = &m_sessionBus;
        call = QDBusMessage::createMethodCall(
            request.owner, QString::fromLatin1(ScreenSaverPath),
            QString::fromLatin1(ScreenSaverInterface), actionMethod(request.action));
    } else if (request.action == SessionAction::Logout) {
        connection = &m_sessionBus;
        call = QDBusMessage::createMethodCall(
            request.owner, QString::fromLatin1(SessionPath),
            QString::fromLatin1(SessionInterface), actionMethod(request.action));
    } else {
        connection = &m_systemBus;
        call = QDBusMessage::createMethodCall(
            request.owner, QString::fromLatin1(LogindPath),
            QString::fromLatin1(LogindInterface), actionMethod(request.action));
        call.setArguments({false});
    }
    m_pending->mutationDispatched = true;
    auto *watcher = new QDBusPendingCallWatcher(connection->asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, request] {
                const QDBusMessage reply = watcher->reply();
                watcher->deleteLater();
                if (!m_pending || m_pending->requestId != request.requestId) {
                    return;
                }
                // AGENT-GUARD: A retired authority cannot confirm the current
                // desktop's action; never replay it on the replacement.
                if (!authorityMatches(request)) {
                    completePending(ActionStatus::Uncertain,
                                    QStringLiteral("authority-replaced"));
                } else if (reply.type() == QDBusMessage::ReplyMessage) {
                    completePending(ActionStatus::Succeeded,
                                    QStringLiteral("applied"));
                } else {
                    completePending(ActionStatus::Uncertain,
                                    QStringLiteral("action-reply-lost"));
                }
            });
}

void SessionActionsClient::completePending(ActionStatus status,
                                           const QString &reasonCode)
{
    if (!m_pending) {
        return;
    }
    const PendingAction request = *m_pending;
    m_pending.reset();
    m_actionDeadline.stop();
    Q_EMIT pendingChanged();
    QString message;
    if (status == ActionStatus::Rejected) {
        message = tr("The session action was not admitted.");
    } else if (status == ActionStatus::Unavailable) {
        message = unavailableText(request.action);
    } else if (status == ActionStatus::Uncertain) {
        message = tr("The session action could not be confirmed and was not replayed.");
    }
    publishFeedback(std::move(message));
    Q_EMIT actionFinished(SessionActionResult{request.requestId, request.action,
                                              status, reasonCode});
    scheduleRefresh();
}

} // namespace QindaQt::Services::SessionActions
