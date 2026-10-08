// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/bluetooth_radio_helper/qt_radio_power_port.h>

#include <QtCore/QHash>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusVirtualObject>
#include <limits>
#include <unistd.h>
#include <utility>

namespace QindaQt::BluetoothRadio {
class QtRadioPowerPort::Private final {
public:
    struct Pending {
        Request request;
        QString helperOwner;
        bool sent = false;
        std::function<bool()> current;
    };
    class IntentObject final : public QDBusVirtualObject {
    public:
        explicit IntentObject(Private &owner) : d(owner) {}
        QString introspect(const QString &) const override {
            return QStringLiteral("<interface name=\"org.qindaqt.BluetoothRadioIntent1\">"
                "<method name=\"Current\"><arg type=\"(ssssst)\" direction=\"in\"/>"
                "<arg type=\"b\" direction=\"out\"/></method></interface>");
        }
        bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override {
            if (message.interface() != QLatin1String(kIntentInterface)
                || message.member() != QLatin1String("Current")) return false;
            if (message.signature() != QLatin1String("(ssssst)") || message.arguments().size() != 1) {
                connection.send(message.createErrorReply(
                    QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
                    QStringLiteral("An exact bounded intent is required")));
                return true;
            }
            if (d.checkingIntent) {
                connection.send(message.createReply(QVariantList{false}));
                return true;
            }
            QScopedValueRollback<bool> checking(d.checkingIntent, true);
            const auto request = qdbus_cast<Request>(message.arguments().constFirst());
            bool allowed = false;
            if (validRequest(request)) {
                for (auto it = d.pending.cbegin(); it != d.pending.cend(); ++it) {
                    if (it->request != request || !it->sent
                        || message.service() != it->helperOwner) continue;
                    const auto callback = it->current;
                    const auto deadline = it->request.deadlineBoottimeMs;
                    const auto helper = it->helperOwner;
                    const auto id = it.key();
                    const auto now = boottimeMilliseconds();
                    if (d.admitted(helper) && now && now < deadline && callback) {
                        try { allowed = callback(); } catch (...) { allowed = false; }
                    }
                    const auto still = d.pending.constFind(id);
                    allowed = allowed && still != d.pending.cend()
                        && still->request == request && still->helperOwner == helper
                        && d.admitted(helper);
                    const auto after = boottimeMilliseconds();
                    allowed = allowed && after && after < deadline;
                    break;
                }
            }
            connection.send(message.createReply(QVariantList{allowed}));
            return true;
        }
    private:
        Private &d;
    };
    Private(QtRadioPowerPort &owner, QDBusConnection connection)
        : q(owner), bus(std::move(connection)), intent(*this) {
        registerDBusTypes();
        endpointReady = bus.registerVirtualObject(QString::fromLatin1(kIntentPath), &intent);
    }
    ~Private() {
        if (endpointReady) bus.unregisterObject(QString::fromLatin1(kIntentPath));
    }
    QtRadioPowerPort &q;
    QDBusConnection bus;
    QHash<quint64, Pending> pending;
    quint64 nextId = 1;
    IntentObject intent;
    bool endpointReady = false;
    bool checkingIntent = false;

    void finish(quint64 id, Disposition disposition, const QString &reason) {
        auto it = pending.find(id);
        if (it == pending.end()) return;
        const auto nonce = it->request.nonce;
        pending.erase(it);
        Q_EMIT q.finished(id, {nonce, disposition, reason});
    }
    void unavailable(quint64 id) {
        finish(id, Disposition::NoWriteUnavailable, QStringLiteral("radio-helper-unavailable"));
    }
    QString currentOwner() const {
        if (!bus.isConnected() || !bus.interface()) return {};
        bus.interface()->setTimeout(250);
        const auto owner = bus.interface()->serviceOwner(QString::fromLatin1(kService));
        return owner.isValid() ? owner.value() : QString{};
    }
    bool admitted(const QString &owner) const {
        if (owner.isEmpty() || currentOwner() != owner) return false;
        const auto uid = bus.interface()->serviceUid(owner);
        return uid.isValid() && uid.value() == geteuid();
    }
    void dispatch(quint64 id) {
        auto it = pending.find(id);
        if (it == pending.end()) return;
        const QString owner = currentOwner();
        if (owner.isEmpty()) { unavailable(id); return; }
        if (!admitted(owner)) {
            finish(id, Disposition::Refused, QStringLiteral("radio-not-authorized"));
            return;
        }
        const auto now = boottimeMilliseconds();
        if (!now || now >= it->request.deadlineBoottimeMs) { unavailable(id); return; }
        it->helperOwner = owner;
        it->sent = true;
        const auto nonce = it->request.nonce;
        auto message = QDBusMessage::createMethodCall(owner, QString::fromLatin1(kPath),
            QString::fromLatin1(kInterface), QStringLiteral("ObserveAndUnblock"));
        message << QVariant::fromValue(it->request);
        auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message,
            static_cast<int>(it->request.deadlineBoottimeMs - now)), &q);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &q,
            [this, watcher, id, owner, nonce] {
                const auto reply = watcher->reply();
                watcher->deleteLater();
                if (!pending.contains(id)) return;
                if (!admitted(owner) || reply.type() != QDBusMessage::ReplyMessage
                    || reply.service() != owner || reply.arguments().size() != 1
                    || reply.signature() != QLatin1String("(sus)")) {
                    finish(id, Disposition::Uncertain, QStringLiteral("radio-change-uncertain"));
                    return;
                }
                const auto result = qdbus_cast<Result>(reply.arguments().constFirst());
                const auto pendingIt = pending.constFind(id);
                const auto replyTime = boottimeMilliseconds();
                if (!validResult(result) || result.nonce != nonce
                    || pendingIt == pending.cend() || !replyTime
                    || replyTime >= pendingIt->request.deadlineBoottimeMs) {
                    finish(id, Disposition::Uncertain, QStringLiteral("radio-change-uncertain"));
                    return;
                }
                finish(id, result.disposition, result.reasonCode);
            });
    }
    void begin(quint64 id) {
        if (!pending.contains(id)) return;
        if (!currentOwner().isEmpty()) { dispatch(id); return; }
        if (!bus.isConnected()) { unavailable(id); return; }
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("StartServiceByName"));
        message << QString::fromLatin1(kService) << quint32(0);
        auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, 750), &q);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &q,
            [this, watcher, id] {
                const auto reply = watcher->reply();
                watcher->deleteLater();
                if (!pending.contains(id)) return;
                if (reply.type() != QDBusMessage::ReplyMessage) { unavailable(id); return; }
                dispatch(id);
            });
    }
};
QtRadioPowerPort::QtRadioPowerPort(QDBusConnection connection, QObject *parent)
    : RadioPowerPort(parent), d(std::make_unique<Private>(*this, std::move(connection))) {}
QtRadioPowerPort::~QtRadioPowerPort() = default;
quint64 QtRadioPowerPort::observeAndUnblock(const QString &bluezOwner,
    const QString &adapterPath, const QString &adapterAddress, const QString &caller,
    std::function<bool()> current) {
    if (!d->endpointReady || !current || d->pending.size() >= 32 || d->nextId == std::numeric_limits<quint64>::max()) return 0;
    const auto now = boottimeMilliseconds();
    if (!now || now > std::numeric_limits<quint64>::max() - kRequestWindowMs) return 0;
    Request request{QUuid::createUuid().toString(QUuid::Id128), bluezOwner,
        adapterPath, adapterAddress, caller, now + kRequestWindowMs};
    if (!validRequest(request)) return 0;
    const quint64 id = d->nextId++;
    d->pending.insert(id, {std::move(request), {}, false, std::move(current)});
    QTimer::singleShot(0, this, [this, id] { d->begin(id); });
    QTimer::singleShot(static_cast<int>(kRequestWindowMs), this, [this, id] {
        const auto it = d->pending.constFind(id);
        if (it == d->pending.cend()) return;
        if (it->sent)
            d->finish(id, Disposition::Uncertain, QStringLiteral("radio-change-uncertain"));
        else d->unavailable(id);
    });
    return id;
}
void QtRadioPowerPort::cancel(quint64 id) { d->pending.remove(id); }
} // namespace QindaQt::BluetoothRadio
