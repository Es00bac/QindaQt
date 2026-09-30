// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/native_notifications.h>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QHash>
#include <QTimer>
#include <unistd.h>
namespace QindaQt::Services::Portal {
namespace { constexpr auto name = "org.freedesktop.Notifications"; constexpr auto path = "/org/freedesktop/Notifications"; }
class QtNativeNotifications::Private {
public:
    QtNativeNotifications &q; QDBusConnection bus; QDBusServiceWatcher watcher;
    QString owner; int pending = 0; QHash<quint32, QString> activations;
    Private(QtNativeNotifications &object, QDBusConnection connection)
        : q(object), bus(connection), watcher(QLatin1String(name), connection,
            QDBusServiceWatcher::WatchForOwnerChange, &object) {
        QObject::connect(&watcher, &QDBusServiceWatcher::serviceOwnerChanged, &q,
            [this](const QString &, const QString &, const QString &) { refresh(); });
        bus.connect({}, QLatin1String(path), QLatin1String(name), QStringLiteral("ActionInvoked"), &q,
                    SLOT(invoked(quint32,QString,QDBusMessage)));
        bus.connect({}, QLatin1String(path), QLatin1String(name), QStringLiteral("ActivationToken"), &q,
                    SLOT(activation(quint32,QString,QDBusMessage)));
        bus.connect({}, QLatin1String(path), QLatin1String(name), QStringLiteral("NotificationClosed"), &q,
                    SLOT(notificationClosed(quint32,quint32,QDBusMessage)));
        refresh();
    }
    QString current() const {
        if (!bus.interface()) return {};
        const auto actor = bus.interface()->serviceOwner(QLatin1String(name));
        if (!actor.isValid() || !actor.value().startsWith(QLatin1Char(':'))) return {};
        const auto uid = bus.interface()->serviceUid(actor.value());
        return uid.isValid() && uid.value() == static_cast<uint>(geteuid()) ? actor.value() : QString{};
    }
    void refresh() {
        const auto next = current(); if (next == owner) return;
        owner = next; activations.clear(); Q_EMIT q.unavailable();
    }
    bool signal(const QDBusMessage &message, const QString &signature) const {
        return !owner.isEmpty() && owner == current() && message.service() == owner
            && message.type() == QDBusMessage::SignalMessage && message.path() == QLatin1String(path)
            && message.interface() == QLatin1String(name) && message.signature() == signature;
    }
};
QtNativeNotifications::QtNativeNotifications(QDBusConnection bus, QObject *parent)
    : NativeNotifications(parent), d(std::make_unique<Private>(*this, bus)) {}
QtNativeNotifications::~QtNativeNotifications() = default;
void QtNativeNotifications::notify(quint64 token, quint32 replaces, const PortalNotification &notification) {
    const auto owner = d->current();
    auto hints = notification.hints;
    if (owner.isEmpty() || owner != d->owner || d->pending >= 32
        || (notification.image.isValid() && !decodeNotificationIcon(notification.image, &hints))) {
        QTimer::singleShot(0, this, [this, token] { Q_EMIT notified(token, false, 0); }); return;
    }
    auto call = QDBusMessage::createMethodCall(owner, QLatin1String(path), QLatin1String(name), QStringLiteral("Notify"));
    call.setArguments({notification.appId.isEmpty() ? QStringLiteral("Local application") : notification.appId,
        replaces, notification.icon, notification.title, notification.body.toHtmlEscaped(), notification.actions, hints, -1});
    ++d->pending;
    auto *watcher = new QDBusPendingCallWatcher(d->bus.asyncCall(call, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, owner, token] {
        const QDBusPendingReply<quint32> reply = *watcher; watcher->deleteLater(); --d->pending;
        const bool valid = owner == d->current() && owner == d->owner && !reply.isError() && reply.value() != 0;
        Q_EMIT notified(token, valid, valid ? reply.value() : 0);
    });
}
void QtNativeNotifications::close(quint64 token, quint32 id) {
    const auto owner = d->current();
    if (!id || owner.isEmpty() || owner != d->owner || d->pending >= 32) {
        QTimer::singleShot(0, this, [this, token] { Q_EMIT removed(token, false); }); return;
    }
    auto call = QDBusMessage::createMethodCall(owner, QLatin1String(path), QLatin1String(name), QStringLiteral("CloseNotification")); call << id;
    ++d->pending; auto *watcher = new QDBusPendingCallWatcher(d->bus.asyncCall(call, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, owner, token] {
        const QDBusPendingReply<> reply = *watcher; watcher->deleteLater(); --d->pending;
        Q_EMIT removed(token, owner == d->current() && owner == d->owner && !reply.isError());
    });
}
void QtNativeNotifications::invoked(quint32 id, const QString &key, const QDBusMessage &message) {
    if (!d->signal(message, QStringLiteral("us")) || key.size() > 255) return;
    const auto activation = d->activations.take(id); Q_EMIT action(id, key, activation);
}
void QtNativeNotifications::activation(quint32 id, const QString &token, const QDBusMessage &message) {
    if (d->signal(message, QStringLiteral("us")) && token.size() <= 512 && (d->activations.contains(id) || d->activations.size() < 64)) d->activations.insert(id, token);
}
void QtNativeNotifications::notificationClosed(quint32 id, quint32, const QDBusMessage &message) {
    if (!d->signal(message, QStringLiteral("uu"))) return;
    d->activations.remove(id); Q_EMIT closed(id);
}
} // namespace QindaQt::Services::Portal
