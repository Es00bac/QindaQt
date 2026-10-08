// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/bluetooth_radio_helper/qt_radio_power_port.h>
#include "radio_service_session_p.h"
#include "native_radio_codec_p.h"
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVirtualObject>
#include <limits>
#include <utility>

namespace QindaQt::BluetoothRadio {
class QtRadioPowerPort::Private final {
public:
    struct Pending {
        Request request;
        QString helperOwner;
        bool sent = false;
        std::function<bool()> current;
        std::unique_ptr<NativeRadioCall> call;
    };
    class IntentObject final : public QDBusVirtualObject {
    public:
        explicit IntentObject(Private &owner) : d(owner) {}
        QString introspect(const QString &) const override {
            return QStringLiteral("<interface name=\"org.qindaqt.BluetoothRadioIntent1\">"
                "<method name=\"Current\"><arg type=\"(ssssstss)\" direction=\"in\"/>"
                "<arg type=\"b\" direction=\"out\"/></method></interface>");
        }
        bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override {
            if (message.interface() != QLatin1String(kIntentInterface)
                || message.member() != QLatin1String("Current")) return false;
            if (message.type() != QDBusMessage::MethodCallMessage
                || message.signature() != QLatin1String(kRequestSignature) || message.arguments().size() != 1) {
                connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
                    QStringLiteral("One complete delegated intent is required")));
                return true;
            }
            if (d.checkingIntent) {
                connection.send(message.createReply(QVariantList{false})); return true;
            }
            d.checkingIntent = true;
            const auto request = qdbus_cast<Request>(message.arguments().constFirst());
            bool allowed = false;
            const QPointer<QtRadioPowerPort> alive(&d.q);
            for (const auto id : d.pending.keys()) {
                const auto item = d.pending.value(id);
                if (!item || !item->sent || item->request != request
                    || message.service() != item->helperOwner) continue;
                allowed = d.admitted(item->helperOwner) && d.current(id, item);
                if (!alive) return true;
                allowed = allowed && d.admitted(item->helperOwner) && d.unexpired(item);
                break;
            }
            d.checkingIntent = false;
            connection.send(message.createReply(QVariantList{allowed}));
            return true;
        }
    private:
        Private &d;
    };
    Private(QtRadioPowerPort &owner, QDBusConnection connection)
        : q(owner), bus(std::move(connection)), intent(*this) { registerDBusTypes(); }
    ~Private() {
        if (wire) wire->setProgress({});
        pending.clear();
        if (endpointReady) bus.unregisterObject(QString::fromLatin1(kIntentPath));
        if (releaseSession) releaseSession();
    }
    QtRadioPowerPort &q;
    QDBusConnection bus;
    NativeRadioWire *wire = nullptr;
    std::function<bool()> sessionCurrent;
    std::function<void()> releaseSession;
    QHash<quint64, std::shared_ptr<Pending>> pending;
    quint64 nextId = 1;
    IntentObject intent;
    bool endpointReady = false, checkingIntent = false;
    bool bind(NativeRadioWire *candidate, std::function<bool()> currentSession) {
        if (!candidate || !candidate->connected()) return false;
        wire = candidate; sessionCurrent = std::move(currentSession);
        endpointReady = bus.registerVirtualObject(QString::fromLatin1(kIntentPath), &intent);
        if (!endpointReady) { wire = nullptr; return false; }
        wire->setProgress([this] { poll(); });
        return true;
    }
    bool unexpired(const std::shared_ptr<Pending> &item) const {
        const auto now = boottimeMilliseconds();
        return now && now < item->request.deadlineBoottimeMs;
    }
    int remaining(const std::shared_ptr<Pending> &item) const {
        const auto now = boottimeMilliseconds();
        if (!now || now >= item->request.deadlineBoottimeMs) return 0;
        return static_cast<int>(item->request.deadlineBoottimeMs - now);
    }
    bool admitted(const QString &owner) const {
        return wire && sessionCurrent && sessionCurrent() && !owner.isEmpty()
            && wire->owner(QString::fromLatin1(kService)) == owner && wire->sameUser(owner);
    }
    bool current(quint64 id, const std::shared_ptr<Pending> &item) {
        if (!item || pending.value(id) != item || !unexpired(item) || !item->current
            || !wire || !sessionCurrent || !sessionCurrent()
            || item->request.authorityOwner != bus.baseService()
            || item->request.transportCaller != wire->uniqueOwner()
            || wire->owner(QStringLiteral("org.qindaqt.Bluetooth1")) != bus.baseService()) return false;
        const auto callback = item->current;
        const QPointer<QtRadioPowerPort> alive(&q);
        bool allowed = false;
        try { allowed = callback(); } catch (...) { allowed = false; }
        if (!alive) return false;
        return allowed && pending.value(id) == item && unexpired(item)
            && sessionCurrent() && wire->owner(QStringLiteral("org.qindaqt.Bluetooth1")) == item->request.authorityOwner;
    }
    void finish(quint64 id, Disposition disposition, const QString &reason) {
        const auto item = pending.take(id);
        if (!item) return;
        Q_EMIT q.finished(id, {item->request.nonce, disposition, reason});
    }
    void uncertain(quint64 id) { finish(id, Disposition::Uncertain, QStringLiteral("radio-change-uncertain")); }
    void unavailable(quint64 id) { finish(id, Disposition::NoWriteUnavailable, QStringLiteral("radio-helper-unavailable")); }
    void dispatch(quint64 id) {
        const auto item = pending.value(id);
        const QPointer<QtRadioPowerPort> alive(&q);
        if (!current(id, item)) { if (alive) finish(id, Disposition::Refused, QStringLiteral("radio-stale-target")); return; }
        const auto owner = wire->owner(QString::fromLatin1(kService));
        if (owner.isEmpty()) { unavailable(id); return; }
        if (!admitted(owner) || !unexpired(item)) {
            finish(id, Disposition::Refused, QStringLiteral("radio-not-authorized")); return;
        }
        auto message = nativeMethod(owner, kPath, kInterface, "ObserveAndUnblock");
        if (!appendNativeRequest(message.get(), item->request)) {
            finish(id, Disposition::Refused, QStringLiteral("radio-request-rejected")); return;
        }
        item->helperOwner = owner;
        // send_with_reply may queue a write even if no pending handle survives.
        item->sent = true;
        item->call = wire->send(std::move(message), remaining(item));
        if (!item->call) { uncertain(id); return; }
        QTimer::singleShot(0, &q, [this] { poll(); });
    }
    void begin(quint64 id) {
        const auto item = pending.value(id);
        if (!item) return;
        if (!wire || !endpointReady) { unavailable(id); return; }
        const QPointer<QtRadioPowerPort> alive(&q);
        if (!current(id, item)) { if (alive) finish(id, Disposition::Refused, QStringLiteral("radio-stale-target")); return; }
        if (!wire->owner(QString::fromLatin1(kService)).isEmpty()) { dispatch(id); return; }
        auto message = nativeMethod(QStringLiteral("org.freedesktop.DBus"),
            "/org/freedesktop/DBus", "org.freedesktop.DBus", "StartServiceByName");
        DBusMessageIter args;
        if (!message) { unavailable(id); return; }
        dbus_message_iter_init_append(message.get(), &args);
        dbus_uint32_t flags = 0;
        if (!appendNativeText(&args, QString::fromLatin1(kService))
            || !dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &flags)) { unavailable(id); return; }
        item->call = wire->send(std::move(message), 750);
        if (!item->call) { unavailable(id); return; }
        QTimer::singleShot(0, &q, [this] { poll(); });
    }
    void poll() {
        // Native dispatch never emits user-facing completion synchronously.
        for (const auto id : pending.keys()) {
            const auto item = pending.value(id);
            if (!item || !item->call || !item->call->completed()) continue;
            auto reply = item->call->take();
            const bool fromPeer = item->call->fromExpectedPeer(reply.get());
            item->call.reset();
            if (!item->sent) {
                dbus_uint32_t activated = 0;
                const bool ready = fromPeer && reply
                    && dbus_message_get_type(reply.get()) == DBUS_MESSAGE_TYPE_METHOD_RETURN
                    && dbus_message_has_signature(reply.get(), "u")
                    && dbus_message_get_args(reply.get(), nullptr, DBUS_TYPE_UINT32, &activated, DBUS_TYPE_INVALID)
                    && (activated == DBUS_START_REPLY_SUCCESS || activated == DBUS_START_REPLY_ALREADY_RUNNING);
                QTimer::singleShot(0, &q, [this, id, ready] { if (ready) dispatch(id); else unavailable(id); });
                continue;
            }
            Result result;
            const bool decoded = fromPeer && readNativeResult(reply.get(), &result)
                && result.nonce == item->request.nonce;
            QTimer::singleShot(0, &q, [this, id, item, decoded, result] {
                if (pending.value(id) != item) return;
                const QPointer<QtRadioPowerPort> alive(&q);
                if (!decoded || !admitted(item->helperOwner) || !current(id, item)) {
                    if (alive) uncertain(id);
                    return;
                }
                if (!alive) return;
                if (!admitted(item->helperOwner) || pending.value(id) != item || !unexpired(item)) {
                    uncertain(id); return;
                }
                finish(id, result.disposition, result.reasonCode);
            });
        }
    }
};
QtRadioPowerPort::QtRadioPowerPort(QDBusConnection connection, QObject *parent)
    : RadioPowerPort(parent), d(std::make_unique<Private>(*this, std::move(connection))) {}
QtRadioPowerPort::QtRadioPowerPort(RadioServiceSession &session, QObject *parent)
    : RadioPowerPort(parent), d(std::make_unique<Private>(*this, session.authorityConnection())) {
    if (session.prepared() && !session.d->portAttached
        && d->bind(session.d->wire.get(), [&session] { return session.prepared(); })) {
        session.d->portAttached = true;
        d->releaseSession = [&session] { session.d->portAttached = false; };
    }
}
QtRadioPowerPort::~QtRadioPowerPort() = default;
quint64 QtRadioPowerPort::observeAndUnblock(const QString &bluezOwner,
    const QString &adapterPath, const QString &adapterAddress, const QString &caller,
    std::function<bool()> current) {
    if (!current || d->pending.size() >= 32 || d->nextId == std::numeric_limits<quint64>::max()) return 0;
    const auto now = boottimeMilliseconds();
    if (!now || now > std::numeric_limits<quint64>::max() - kRequestWindowMs) return 0;
    auto item = std::make_shared<Private::Pending>();
    item->request = {QUuid::createUuid().toString(QUuid::Id128), bluezOwner,
        adapterPath, adapterAddress, caller, now + kRequestWindowMs,
        d->bus.baseService(), d->wire ? d->wire->uniqueOwner() : QString{}};
    if (d->wire && !validRequest(item->request)) return 0;
    item->current = std::move(current);
    const auto id = d->nextId++;
    d->pending.insert(id, item);
    QTimer::singleShot(0, this, [this, id] { d->begin(id); });
    QTimer::singleShot(static_cast<int>(kRequestWindowMs), this, [this, id] {
        const auto timedItem = d->pending.value(id);
        if (!timedItem) return;
        if (timedItem->sent) d->uncertain(id); else d->unavailable(id);
    });
    return id;
}
void QtRadioPowerPort::cancel(quint64 id) { d->pending.remove(id); }
} // namespace QindaQt::BluetoothRadio
