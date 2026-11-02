// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_notifications/fdo_phone_presenter.h>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

namespace QindaQt::Link {
namespace {
constexpr auto kService = "org.freedesktop.Notifications";
constexpr auto kPath = "/org/freedesktop/Notifications";

QDBusMessage call(const char *method)
{
    return QDBusMessage::createMethodCall(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                                          QString::fromLatin1(kService), QString::fromLatin1(method));
}
} // namespace

FdoPhoneNotificationPresenter::FdoPhoneNotificationPresenter(QDBusConnection bus, QObject *parent)
    : NotificationPresenter(parent), m_bus(std::move(bus))
{
    m_bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath), QString::fromLatin1(kService),
                  QStringLiteral("ActionInvoked"), this, SLOT(onActionInvoked(uint, QString)));
    m_bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath), QString::fromLatin1(kService),
                  QStringLiteral("NotificationClosed"), this, SLOT(onClosed(uint, uint)));
}

quint32 FdoPhoneNotificationPresenter::present(const PresentedNotification &n, quint32 replaces)
{
    const bool update = replaces != 0 && m_slots.contains(replaces) && m_slots.value(replaces).serverId != 0;
    const quint32 handle = update ? replaces : ++m_next;
    if (!update) m_slots.insert(handle, Slot{});
    QStringList flat;
    for (const auto &a : n.actions) flat << a.first << a.second;
    QVariantMap hints{{QStringLiteral("category"), QStringLiteral("im.received")}};
    if (!n.iconPath.isEmpty()) hints.insert(QStringLiteral("image-path"), n.iconPath);
    if (n.quiet) hints.insert(QStringLiteral("suppress-sound"), true);
    QDBusMessage m = call("Notify");
    m << n.appName << (update ? m_slots.value(handle).serverId : 0u) << n.iconPath << n.summary << n.body << flat
      << hints << -1;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(m), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, handle](QDBusPendingCallWatcher *w) {
        const QDBusPendingReply<uint> reply = *w;
        w->deleteLater();
        auto slot = m_slots.find(handle);
        if (slot == m_slots.end() || !reply.isValid()) return;
        slot->serverId = reply.value();
        m_byServerId.insert(reply.value(), handle);
        if (slot->closeWhenKnown) close(handle);
    });
    return handle;
}

void FdoPhoneNotificationPresenter::close(quint32 handle)
{
    const auto slot = m_slots.find(handle);
    if (slot == m_slots.end()) return;
    if (slot->serverId == 0) {
        slot->closeWhenKnown = true;  // the Notify reply has not arrived yet
        return;
    }
    QDBusMessage m = call("CloseNotification");
    m << slot->serverId;
    m_byServerId.remove(slot->serverId);
    m_slots.erase(slot);
    m_bus.asyncCall(m);
}

void FdoPhoneNotificationPresenter::onActionInvoked(uint serverId, const QString &key)
{
    const quint32 handle = m_byServerId.value(serverId, 0);
    if (handle != 0) Q_EMIT actionInvoked(handle, key);
}

void FdoPhoneNotificationPresenter::onClosed(uint serverId, uint reason)
{
    const quint32 handle = m_byServerId.take(serverId);
    if (handle == 0) return;
    m_slots.remove(handle);
    if (reason == 2) Q_EMIT userDismissed(handle);
}

} // namespace QindaQt::Link
