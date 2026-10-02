// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/session_actions/session_actions_client.h>
#include "session_actions_internal.h"
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
namespace QindaQt::Services::SessionActions {
using namespace Detail;
void SessionActionsClient::sleepAvailabilityChanged(const QDBusMessage &message) {
    if (m_running && message.type() == QDBusMessage::SignalMessage &&
        message.signature().isEmpty() &&
        message.service() == currentOwner(SessionAction::Suspend)) scheduleRefresh();
}
void SessionActionsClient::authorizeSuspend() {
    const PendingAction request = *m_pending;
    auto call = QDBusMessage::createMethodCall(request.owner, QString::fromLatin1(SleepPath),
        QString::fromLatin1(SleepInterface), QStringLiteral("Can") + sleepMethod(request.action));
    auto *watcher = new QDBusPendingCallWatcher(m_sessionBus.asyncCall(call, 750), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, request] {
        const QDBusPendingReply<bool> reply = *watcher;
        watcher->deleteLater();
        if (!m_pending || m_pending->requestId != request.requestId) return;
        if (!authorityMatches(request)) {
            completePending(ActionStatus::Unavailable, QStringLiteral("authority-replaced"));
        } else if (reply.isError() || !reply.value()) {
            completePending(ActionStatus::Rejected, QStringLiteral("native-sleep-not-admitted"));
        } else {
            dispatchMutation();
        }
    });
}
}
