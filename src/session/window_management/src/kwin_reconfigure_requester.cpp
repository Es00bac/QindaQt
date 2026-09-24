// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/window_management/kwin_reconfigure_requester.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusConnectionInterface>
#include <QDBusReply>

#include <utility>

namespace QindaQt::Session::WindowManagement {

DBusKWinReconfigureRequester::DBusKWinReconfigureRequester(QDBusConnection bus, QObject *parent)
    : KWinReconfigureRequester(parent)
    , m_bus(std::move(bus))
{
    if (m_bus.interface() != nullptr) {
        const QDBusReply<QString> owner = m_bus.interface()->serviceOwner(QStringLiteral("org.kde.KWin"));
        if (owner.isValid()) {
            m_owner = owner.value();
        }
    }
    m_serviceWatcher = new QDBusServiceWatcher(
        QStringLiteral("org.kde.KWin"), m_bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &newOwner) {
                m_owner = newOwner;
                Q_EMIT ownerChanged(m_owner);
            });
}

void DBusKWinReconfigureRequester::requestReconfigure(quint64 requestId)
{
    ++m_requests;
    const QString owner = m_owner;
    if (owner.isEmpty()) {
        Q_EMIT reconfigureFinished(requestId, owner,
                                   QStringLiteral("KWin is not running in this session."));
        return;
    }
    QDBusMessage request = QDBusMessage::createMethodCall(
        owner, QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"),
        QStringLiteral("reconfigure"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(request, 5'000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, requestId, owner](QDBusPendingCallWatcher *done) {
        const QDBusPendingReply<> reply = *done;
        QString error = reply.isError() ? reply.error().message().left(256) : QString{};
        if (error.isEmpty() && owner != m_owner) {
            error = QStringLiteral("KWin changed while its reconfigure request was in flight.");
        }
        Q_EMIT reconfigureFinished(requestId, owner, error);
        done->deleteLater();
    });
}

} // namespace QindaQt::Session::WindowManagement
