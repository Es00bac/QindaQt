// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/notification_adaptor.h>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QDBusPendingCall>
#include <QElapsedTimer>
#include <QHash>
#include <QRegularExpression>
#include <utility>
namespace QindaQt::Services::Portal {
class NotificationAdaptor::Private {
public:
    struct Active { quint32 nativeId = 0; PortalNotification notification; };
    struct Pending { QDBusMessage call; QString key; PortalNotification notification; bool remove = false, retired = false; };
    NotificationAdaptor &q; RequestRegistry &admission; NativeNotifications &native; QDBusConnection bus;
    QDBusServiceWatcher watcher; QElapsedTimer clock; quint64 sequence = 0;
    QHash<QString, Active> active; QHash<quint64, Pending> pending;
    int rate = 0;
    Private(NotificationAdaptor &object, RequestRegistry &gate, NativeNotifications &port, QDBusConnection connection)
        : q(object), admission(gate), native(port), bus(connection), watcher(QStringLiteral("org.freedesktop.portal.Desktop"), connection,
            QDBusServiceWatcher::WatchForOwnerChange, &object) {
        clock.start(); registerNotificationTypes();
        QObject::connect(&watcher, &QDBusServiceWatcher::serviceOwnerChanged, &q,
            [this](const QString &, const QString &old, const QString &next) { if (!old.isEmpty() && old != next) retire(); });
        QObject::connect(&native, &NativeNotifications::unavailable, &q, [this] { retire(false); });
        QObject::connect(&native, &NativeNotifications::notified, &q, [this](quint64 token, bool success, quint32 id) {
            const auto it = pending.find(token); if (it == pending.end() || it->remove) return;
            auto request = std::move(it.value()); pending.erase(it);
            if (request.retired || !admission.authenticated(request.call)) {
                if (success && id) native.close(++sequence, id);
                return;
            }
            if (success && id) {
                request.notification.image = {}; // Drop received icon FD after native transfer.
                active.insert(request.key, {id, std::move(request.notification)});
            }
            reply(request.call, success && id);
        });
        QObject::connect(&native, &NativeNotifications::removed, &q, [this](quint64 token, bool success) {
            const auto it = pending.find(token); if (it == pending.end() || !it->remove) return;
            auto request = std::move(it.value()); pending.erase(it);
            if (request.retired) return;
            if (success) active.remove(request.key);
            reply(request.call, success && admission.authenticated(request.call));
        });
        QObject::connect(&native, &NativeNotifications::closed, &q, [this](quint32 id) {
            for (auto it = active.begin(); it != active.end();) {
                if (it->nativeId == id) it = active.erase(it); else ++it;
            }
        });
        QObject::connect(&native, &NativeNotifications::action, &q,
            [this](quint32 id, const QString &key, const QString &activation) { invoked(id, key, activation); });
    }
    ~Private() { retire(); }
    void reply(const QDBusMessage &call, bool success) {
        bus.send(success ? call.createReply() : call.createErrorReply(QStringLiteral("org.freedesktop.portal.Error.Failed"), QStringLiteral("Notification unavailable")));
    }
    void retire(bool closeNative = true) {
        for (auto &request : pending) if (!request.retired) { request.retired = true; reply(request.call, false); }
        const auto old = std::exchange(active, {});
        if (closeNative) for (const auto &entry : old) native.close(++sequence, entry.nativeId);
    }
    bool admit(const QDBusMessage &call, const QString &key) {
        call.setDelayedReply(true);
        if (!admission.authenticated(call)) {
            bus.send(call.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"), QStringLiteral("Frontend unavailable"))); return false;
        }
        if (clock.elapsed() >= 60000) { rate = 0; clock.restart(); }
        if (++rate > 128 || pending.size() >= 32) { reply(call, false); return false; }
        for (const auto &request : std::as_const(pending)) if (request.key == key) { reply(call, false); return false; }
        return true;
    }
    static QString identity(const QString &app, const QString &id) {
        // Length-prefixing remains collision-safe even if an id contains a
        // separator; app identity is never normalized or joined ambiguously.
        return QString::number(app.size()) + QLatin1Char(':') + app + id;
    }
    void invoked(quint32 id, const QString &key, const QString &activation) {
        for (const auto &entry : std::as_const(active)) {
            if (entry.nativeId != id || !entry.notification.actionValues.contains(key)) continue;
            const auto action = entry.notification.actionValues[key]; const auto &notification = entry.notification;
            const auto frontend = admission.frontendOwner(); if (frontend.isEmpty()) return;
            QVariantMap platform; if (!activation.isEmpty()) platform.insert(QStringLiteral("activation-token"), activation);
            QVariantList parameters; if (action.target.isValid()) parameters.append(QVariant::fromValue(QDBusVariant(action.target)));
            static const QRegularExpression appBus(QStringLiteral("^[A-Za-z_][A-Za-z0-9_-]*(\\.[A-Za-z_][A-Za-z0-9_-]*)+$"));
            if (action.name.startsWith(QStringLiteral("app."))) {
                if (!appBus.match(notification.appId).hasMatch()) return;
                QString path = QLatin1Char('/') + notification.appId; path.replace(QLatin1Char('.'), QLatin1Char('/')); path.replace(QLatin1Char('-'), QLatin1Char('_'));
                auto call = QDBusMessage::createMethodCall(notification.appId, path,
                    QStringLiteral("org.freedesktop.Application"), QStringLiteral("ActivateAction"));
                call.setArguments({action.name.mid(4), parameters, platform}); bus.asyncCall(call, 3000); return;
            }
            parameters.append(QVariant::fromValue(QDBusVariant(platform)));
            auto signal = QDBusMessage::createTargetedSignal(frontend, QStringLiteral("/org/freedesktop/portal/desktop"),
                QStringLiteral("org.freedesktop.impl.portal.Notification"), QStringLiteral("ActionInvoked"));
            signal.setArguments({notification.appId, notification.id, action.name, parameters}); bus.send(signal); return;
        }
    }
};
NotificationAdaptor::NotificationAdaptor(QObject &host, RequestRegistry &registry, NativeNotifications &native, QDBusConnection bus)
    : QDBusAbstractAdaptor(&host), d(std::make_unique<Private>(*this, registry, native, bus)) { setAutoRelaySignals(false); }
NotificationAdaptor::~NotificationAdaptor() = default;
void NotificationAdaptor::AddNotification(const QString &app, const QString &id, const QVariantMap &map, const QDBusMessage &call) {
    const auto key = Private::identity(app, id); if (!d->admit(call, key)) return;
    const auto notification = portalNotification(app, id, map);
    int appCount = 0; for (const auto &entry : std::as_const(d->active)) if (entry.notification.appId == app) ++appCount;
    qsizetype reserved = 0;
    // Outstanding additions reserve capacity before native publication; queued
    // replies must not let one app or the global map exceed the active bound.
    for (const auto &entry : std::as_const(d->pending)) {
        if (entry.remove || entry.retired || d->active.contains(entry.key)) continue;
        ++reserved; if (entry.notification.appId == app) ++appCount;
    }
    if (!notification || (!d->active.contains(key) && (d->active.size() + reserved >= 64 || appCount >= 8))) { d->reply(call, false); return; }
    const auto token = ++d->sequence; const auto replaces = d->active.value(key).nativeId;
    d->pending.insert(token, {call, key, *notification, false, false}); d->native.notify(token, replaces, *notification);
}
void NotificationAdaptor::RemoveNotification(const QString &app, const QString &id, const QDBusMessage &call) {
    // Reuse the owning identity policy before constructing a retained key.
    if (!portalNotification(app, id, {})) {
        call.setDelayedReply(true); d->reply(call, false); return;
    }
    const auto key = Private::identity(app, id); if (!d->admit(call, key)) return;
    if (!d->active.contains(key)) { d->reply(call, true); return; }
    const auto token = ++d->sequence; const auto nativeId = d->active[key].nativeId;
    d->pending.insert(token, {call, key, {}, true, false}); d->native.close(token, nativeId);
}
} // namespace QindaQt::Services::Portal
