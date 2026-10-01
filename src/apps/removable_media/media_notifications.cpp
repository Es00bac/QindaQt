// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_notifications.h"
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <utility>

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
        auto affected = m_pendingTokens;
        for (const auto &token : std::as_const(m_tokens)) affected.insert(token);
        ++m_epoch; m_owner = owner; m_tokens.clear(); m_sequences.clear(); m_pendingTokens.clear();
        // AGENT-GUARD: losing Notify's owner must not consume the sole
        // insertion prompt. show() rejects attachments already withdrawn.
        for (const auto &token : std::as_const(affected)) m_controller.show(token);
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
    m_pendingTokens.insert(token);
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
        if (epoch != m_epoch) return;
        if (sequence != m_sequences.value(token)) {
            // Unplug may arrive before Notify's id. Withdraw the eventual
            // notification too, rather than leaving a dead action on screen.
            // Updates may share the previous id. Closing that id would also
            // close the current update; withdrawal removes its mapping first.
            if (!reply.isError() && m_tokens.value(reply.value()) != token) {
                auto closeMessage = QDBusMessage::createMethodCall(m_owner, Path, Service, QStringLiteral("CloseNotification"));
                closeMessage.setArguments({reply.value()});
                m_bus.asyncCall(closeMessage, 5000);
            }
            return;
        }
        m_pendingTokens.remove(token);
        if (reply.isError()) { m_controller.show(token); return; }
        m_tokens.remove(previous);
        m_tokens.insert(reply.value(), token);
    });
}
void MediaNotifications::withdraw(const QString &token)
{
    ++m_sequences[token];
    m_pendingTokens.remove(token);
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
