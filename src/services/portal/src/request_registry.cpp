// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/request_registry.h>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QHash>
#include <QRegularExpression>
#include <QTimer>
#include <unistd.h>

namespace QindaQt::Services::Portal {
namespace {
constexpr auto frontend = "org.freedesktop.portal.Desktop";
constexpr auto requestInterface = "org.freedesktop.impl.portal.Request";
class CloseObject final : public QDBusVirtualObject {
public:
    CloseObject(std::function<void(const QDBusMessage &)> close, QObject *parent)
        : QDBusVirtualObject(parent), m_close(std::move(close)) {}
    QString introspect(const QString &) const override {
        return QStringLiteral("<interface name='org.freedesktop.impl.portal.Request'>"
                              "<method name='Close'/></interface>");
    }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &) override {
        if (message.interface() != QLatin1String(requestInterface)
            || message.member() != QStringLiteral("Close")) return false;
        m_close(message);
        return true;
    }
private:
    std::function<void(const QDBusMessage &)> m_close;
};
bool validPath(const QString &path) {
    static const QRegularExpression expression(QStringLiteral(
        "^/org/freedesktop/portal/desktop/request/[A-Za-z0-9_]+/[A-Za-z0-9_]+$"));
    return path.size() <= 512 && expression.match(path).hasMatch();
}
bool validApp(const QString &app) {
    if (app.size() > 255) return false;
    for (const QChar c : app) if (c.isNull() || c.category() == QChar::Other_Control) return false;
    return true;
}
}
class RequestRegistry::Private {
public:
    struct Entry {
        QDBusMessage call;
        QString owner;
        QString path;
        Retirement retirement;
        CloseObject *object = nullptr;
        QTimer *timer = nullptr;
        bool held = false;
        bool voidMethod = false;
    };
    struct Budget { qint64 start = 0; int count = 0; };
    QDBusConnection bus;
    QDBusServiceWatcher watcher;
    QElapsedTimer clock;
    QHash<RequestToken, Entry> entries;
    QHash<QString, Budget> budgets;
    RequestToken sequence = 0;
    explicit Private(QDBusConnection connection, QObject *parent)
        : bus(connection), watcher(QLatin1String(frontend), connection,
              QDBusServiceWatcher::WatchForOwnerChange, parent) { clock.start(); }
};
RequestRegistry::RequestRegistry(QDBusConnection bus, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(bus, this)) {
    connect(&d->watcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &RequestRegistry::frontendChanged);
}
RequestRegistry::~RequestRegistry() { retireAll(); }
QString RequestRegistry::frontendOwner() const {
    auto *interface = d->bus.interface();
    if (!interface) return {};
    const QDBusReply<QString> owner = interface->serviceOwner(QLatin1String(frontend));
    if (!owner.isValid() || !owner.value().startsWith(QLatin1Char(':'))) return {};
    const QDBusReply<uint> uid = interface->serviceUid(owner.value());
    return uid.isValid() && uid.value() == static_cast<uint>(geteuid()) ? owner.value() : QString{};
}
bool RequestRegistry::authenticated(const QDBusMessage &call) const {
    return call.type() == QDBusMessage::MethodCallMessage && !call.service().isEmpty()
        && call.service() == frontendOwner();
}
RequestToken RequestRegistry::begin(const QDBusMessage &call, const QString &path,
                                    const QString &app, Retirement retirement, int deadlineMs, bool voidMethod) {
    QString error;
    if (!authenticated(call)) error = QStringLiteral("org.freedesktop.DBus.Error.AccessDenied");
    else if (!validPath(path) || !validApp(app) || deadlineMs < 1 || deadlineMs > 120000)
        error = QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs");
    else if (d->entries.size() >= 32) error = QStringLiteral("org.freedesktop.portal.Error.LimitsExceeded");
    for (const auto &entry : std::as_const(d->entries))
        if (entry.path == path) error = QStringLiteral("org.freedesktop.portal.Error.Exists");
    if (error.isEmpty()) {
        // The frontend is the actor even for an empty non-sandbox app id; no
        // anonymous cross-actor authority is derived from this budget key.
        const QString key = call.service() + QLatin1Char('\n') + app;
        if (!d->budgets.contains(key) && d->budgets.size() >= 128)
            error = QStringLiteral("org.freedesktop.portal.Error.LimitsExceeded");
        else {
            auto &budget = d->budgets[key];
            if (d->clock.elapsed() - budget.start >= 60000) budget = {d->clock.elapsed(), 0};
            if (++budget.count > 32) error = QStringLiteral("org.freedesktop.portal.Error.LimitsExceeded");
        }
    }
    if (!error.isEmpty()) { d->bus.send(call.createErrorReply(error, QStringLiteral("Request refused"))); return 0; }
    const RequestToken token = ++d->sequence;
    auto *object = new CloseObject([this, token](const QDBusMessage &close) {
        if (!close.signature().isEmpty() || !authenticated(close) || !live(token)) {
            d->bus.send(close.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                                                QStringLiteral("Close refused")));
            return;
        }
        d->bus.send(close.createReply());
        retire(token, RequestResponse::Cancelled);
    }, this);
    if (!d->bus.registerVirtualObject(path, object, QDBusConnection::SingleNode)) {
        delete object;
        d->bus.send(call.createErrorReply(QStringLiteral("org.freedesktop.portal.Error.Failed"),
                                          QStringLiteral("Request unavailable")));
        return 0;
    }
    call.setDelayedReply(true);
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    d->entries.insert(token, {call, call.service(), path, std::move(retirement), object, timer, false, voidMethod});
    connect(timer, &QTimer::timeout, this, [this, token] { retire(token, RequestResponse::Failed); });
    timer->start(deadlineMs);
    return token;
}
bool RequestRegistry::live(RequestToken token) const {
    const auto it = d->entries.constFind(token);
    return it != d->entries.cend() && it->owner == frontendOwner();
}
void RequestRegistry::finish(RequestToken token, RequestResponse response, const QVariantMap &results) {
    const auto it = d->entries.find(token);
    if (it == d->entries.end()) return;
    auto entry = std::move(it.value());
    d->entries.erase(it);
    d->bus.unregisterObject(entry.path);
    entry.timer->stop(); entry.timer->deleteLater(); entry.object->deleteLater();
    // Recheck the actor at publication. A stale owner gets no successful data.
    if (entry.owner != frontendOwner()) response = RequestResponse::Failed;
    if (!entry.held) {
        if (entry.voidMethod) d->bus.send(entry.call.createErrorReply(
            response == RequestResponse::Cancelled ? QStringLiteral("org.freedesktop.portal.Error.Cancelled")
                : QStringLiteral("org.freedesktop.portal.Error.Failed"), QStringLiteral("Request retired")));
        else d->bus.send(entry.call.createReply({static_cast<quint32>(response),
            response == RequestResponse::Success ? results : QVariantMap{}}));
    }
    if (entry.retirement) entry.retirement(response);
}
void RequestRegistry::acknowledgeHeld(RequestToken token) {
    auto it = d->entries.find(token);
    if (it == d->entries.end()) return;
    if (!live(token)) { failVoid(token, QStringLiteral("org.freedesktop.portal.Error.Failed")); return; }
    it->held = true; it->timer->stop(); d->bus.send(it->call.createReply());
}
void RequestRegistry::failVoid(RequestToken token, const QString &name) {
    auto it = d->entries.find(token);
    if (it == d->entries.end()) return;
    if (!it->held) d->bus.send(it->call.createErrorReply(name, QStringLiteral("Inhibitor unavailable")));
    it->held = true;
    finish(token, RequestResponse::Failed);
}
void RequestRegistry::retire(RequestToken token, RequestResponse reason) { finish(token, reason); }
void RequestRegistry::retireAll(RequestResponse reason) {
    const auto tokens = d->entries.keys();
    for (const auto token : tokens) retire(token, reason);
}
void RequestRegistry::frontendChanged(const QString &, const QString &oldOwner, const QString &newOwner) {
    if (!oldOwner.isEmpty() && oldOwner != newOwner) { retireAll(); d->budgets.clear(); }
}
} // namespace QindaQt::Services::Portal
