// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/window_management/kwin_reconfigure_requester.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>

#include <utility>

namespace QindaQt::Session::WindowManagement {

DBusKWinReconfigureRequester::DBusKWinReconfigureRequester(QDBusConnection bus, QObject *parent)
    : QObject(parent)
    , m_bus(std::move(bus))
{
}

void DBusKWinReconfigureRequester::requestReconfigure()
{
    ++m_requests;
    QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"),
        QStringLiteral("reconfigure"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(request, 5'000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *done) {
        const QDBusPendingReply<> reply = *done;
        if (reply.isError()) {
            qWarning().noquote() << "QindaQt session could not ask KWin to reconfigure:"
                                 << reply.error().message().left(256);
        }
        done->deleteLater();
    });
}

} // namespace QindaQt::Session::WindowManagement
