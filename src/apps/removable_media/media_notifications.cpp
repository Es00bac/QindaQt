// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_notifications.h"
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace QindaQt::Apps::RemovableMedia {
namespace {
const QString Service = QStringLiteral("org.freedesktop.Notifications");
const QString Path = QStringLiteral("/org/freedesktop/Notifications");
}
MediaNotifications::MediaNotifications(MediaController &controller, QDBusConnection connection,
                                       QObject *parent)
    : QObject(parent), m_controller(controller), m_bus(std::move(connection)),
      m_watcher(Service, m_bus, QDBusServiceWatcher::WatchForOwnerChange)
{
    if (m_bus.interface()) m_owner = m_bus.interface()->serviceOwner(Service).value();
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &owner) {
        ++m_epoch; m_owner = owner; m_tokens.clear(); m_sequences.clear();
    });
    connect(&controller, &MediaController::notificationRequested, this, &MediaNotifications::notify);
    connect(&controller, &MediaController::notificationWithdrawn, this, &MediaNotifications::withdraw);
    m_bus.connect(Service, Path, Service, QStringLiteral("ActionInvoked"), this,
                  SLOT(actionInvoked(uint,QString,QDBusMessage)));
    m_bus.connect(Service, Path, Service, QStringLiteral("NotificationClosed"), this,
                  SLOT(closed(uint,uint,QDBusMessage)));
}
void MediaNotifications::notify(const QString &token, const QString &summary,
                                const QString &body, const QStringList &actions)
{
    if (m_owner.isEmpty()) { m_controller.show(token); return; }
    uint previous = 0;
    for (auto it = m_tokens.cbegin(); it != m_tokens.cend(); ++it)
        if (it.value() == token) { previous = it.key(); break; }
    const auto epoch = m_epoch;
    const auto sequence = ++m_sequences[token];
    auto message = QDBusMessage::createMethodCall(m_owner, Path, Service, QStringLiteral("Notify"));
    message.setArguments({QStringLiteral("Removable Media"), previous, QStringLiteral("drive-removable-media"),
        summary.toHtmlEscaped(), body.toHtmlEscaped(), actions,
        QVariantMap{{QStringLiteral("desktop-entry"), QStringLiteral("org.qindaqt.RemovableMedia")},
                    {QStringLiteral("resident"), true}}, -1});
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, epoch, sequence, token, previous](QDBusPendingCallWatcher *pending) {
        const QDBusPendingReply<uint> reply = *pending;
        pending->deleteLater();
        if (epoch != m_epoch || sequence != m_sequences.value(token)) return;
        if (reply.isError()) { m_controller.show(token); return; }
        m_tokens.remove(previous);
        m_tokens.insert(reply.value(), token);
    });
}
void MediaNotifications::withdraw(const QString &token)
{
    ++m_sequences[token];
    for (auto it = m_tokens.begin(); it != m_tokens.end();) {
        if (it.value() != token) { ++it; continue; }
        auto message = QDBusMessage::createMethodCall(m_owner, Path, Service, QStringLiteral("CloseNotification"));
        message.setArguments({it.key()});
        m_bus.asyncCall(message, 5000);
        it = m_tokens.erase(it);
    }
}
void MediaNotifications::actionInvoked(uint id, const QString &action, const QDBusMessage &message)
{
    if (message.service() != m_owner || !m_tokens.contains(id)) return;
    m_controller.notificationAction(m_tokens.value(id), action);
}
void MediaNotifications::closed(uint id, uint, const QDBusMessage &message)
{
    if (message.service() == m_owner) m_tokens.remove(id);
}
} // namespace QindaQt::Apps::RemovableMedia
