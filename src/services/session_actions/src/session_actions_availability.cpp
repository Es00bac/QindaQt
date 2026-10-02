// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/session_actions/session_actions_client.h>
#include "session_actions_internal.h"
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
namespace QindaQt::Services::SessionActions {
using namespace Detail;
struct SessionActionsClient::RefreshQuery final {
    quint64 serial = 0;
    quint64 screenSaverEpoch = 0;
    quint64 sessionEpoch = 0;
    quint64 logindEpoch = 0;
    quint64 sleepEpoch = 0;
    int outstanding = 0;
    SessionActionAvailability availability;
};

void SessionActionsClient::refreshAvailability()
{
    if (!m_running) {
        return;
    }
    const quint64 serial = ++m_refreshSerial;
    refreshScreenOffAvailability();
    auto query = std::make_shared<RefreshQuery>();
    query->serial = serial;

    const QString screenSaverOwner = serviceOwner(m_sessionBus, ScreenSaverService);
    const QString sessionOwner = serviceOwner(m_sessionBus, SessionService);
    const QString logindOwner = serviceOwner(m_systemBus, LogindService);
    query->screenSaverEpoch = m_screenSaverEpoch;
    query->sessionEpoch = m_sessionEpoch;
    query->logindEpoch = m_logindEpoch;
    query->sleepEpoch = m_sleepEpoch;
    const QString sleepOwner = currentOwner(SessionAction::Suspend);
    query->outstanding = (screenSaverOwner.isEmpty() ? 0 : 1)
        + (sessionOwner.isEmpty() ? 0 : 1)
        + (logindOwner.isEmpty() ? 0 : 2)
        + (sleepOwner.isEmpty() ? 0 : 2);
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

    for (const auto mode : {SessionAction::Suspend, SessionAction::Hibernate}) {
        if (sleepOwner.isEmpty()) break;
        auto call = QDBusMessage::createMethodCall(sleepOwner, QString::fromLatin1(SleepPath),
            QString::fromLatin1(SleepInterface), QStringLiteral("Can") + sleepMethod(mode));
        auto *watcher = new QDBusPendingCallWatcher(m_sessionBus.asyncCall(call, 750), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, query, sleepOwner, mode] {
                const QDBusPendingReply<bool> reply = *watcher;
                watcher->deleteLater();
                if (query->serial != m_refreshSerial || !m_running) return;
                const bool admitted = !reply.isError() && reply.value() &&
                    currentOwner(SessionAction::Suspend) == sleepOwner &&
                    query->sleepEpoch == m_sleepEpoch;
                if (mode == SessionAction::Suspend) query->availability.suspend = admitted;
                else query->availability.hibernate = admitted;
                completeRefresh(query);
            });
    }

    for (const SessionAction action : {SessionAction::Reboot,
                                       SessionAction::PowerOff}) {
        if (logindOwner.isEmpty()) {
            break;
        }
        QDBusMessage call = QDBusMessage::createMethodCall(
            logindOwner, QString::fromLatin1(LogindPath),
            QString::fromLatin1(LogindInterface), (action == SessionAction::Reboot ? QStringLiteral("CanReboot") : QStringLiteral("CanPowerOff")));
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
                    case SessionAction::Hibernate:
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

}
