// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/shortcuts_client/shortcut_lease.h"
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
namespace QindaQt::Services::Shortcuts {
namespace {
QStringList texts(const QList<QKeySequence> &keys) { QStringList result; for (const auto &key : keys) result << key.toString(QKeySequence::PortableText); return result; }
bool accepted(const QDBusMessage &reply) { return reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty() && reply.arguments().first().metaType() == QMetaType::fromType<bool>() && reply.arguments().first().toBool(); }
}
ShortcutLease::ShortcutLease(QDBusConnection bus, QString component, QObject *parent)
    : QObject(parent), m_bus(std::move(bus)), m_watch(QLatin1String(Service), m_bus, QDBusServiceWatcher::WatchForOwnerChange, this), m_component(std::move(component)) {
    connect(&m_watch, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &owner) { attach(owner); });
    resolveOwner();
}
ShortcutLease::~ShortcutLease() {
    if (m_owner.isEmpty()) return;
    for (const auto &action : m_registered) {
        auto call = QDBusMessage::createMethodCall(m_owner, QLatin1String(Path), QLatin1String(Service), "Unregister");
        call.setArguments({m_component, action}); m_bus.asyncCall(call, 1000);
    }
}
void ShortcutLease::resolveOwner() {
    const auto generation = m_generation;
    auto call = QDBusMessage::createMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetNameOwner");
    call.setArguments({QLatin1String(Service)});
    auto *watch = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 1500), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, generation](auto *w) {
        const QDBusPendingReply<QString> reply = *w; w->deleteLater();
        if (generation == m_generation) attach(reply.isError() ? QString{} : reply.value());
    });
}
void ShortcutLease::attach(const QString &owner) {
    if (!m_owner.isEmpty()) m_bus.disconnect(m_owner, QLatin1String(Path), QLatin1String(Service), {}, this, nullptr);
    ++m_generation; m_owner = owner; m_busy = false; m_observing = false; m_observeAgain = false; m_registered.clear(); m_ready.clear();
    if (owner.isEmpty()) { for (const auto &binding : m_desired) Q_EMIT status(binding.action, "unavailable", {}); return; }
    m_bus.connect(owner, QLatin1String(Path), QLatin1String(Service), "Activated", this, SLOT(activatedOnBus(QString,QString,quint64)));
    m_bus.connect(owner, QLatin1String(Path), QLatin1String(Service), "BindingsChanged", this, SLOT(bindingsChanged()));
    reconcile();
}
void ShortcutLease::replace(QList<Binding> desired, QSet<QString> assign) {
    if (desired.size() > 64) return;
    ++m_generation; m_desired = std::move(desired); m_assign.unite(assign); m_ready.clear(); m_errors.clear();
    m_busy = false; m_observing = false;
    if (m_owner.isEmpty()) resolveOwner();
    reconcile();
}
void ShortcutLease::request(const QString &method, const QVariantList &args, std::function<void(const QDBusMessage &)> done) {
    const auto generation = m_generation; const auto owner = m_owner;
    auto call = QDBusMessage::createMethodCall(owner, QLatin1String(Path), QLatin1String(Service), method); call.setArguments(args);
    auto *watch = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 2000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, generation, owner, done = std::move(done)](auto *w) {
        const QDBusMessage reply = w->reply(); w->deleteLater();
        if (generation == m_generation && owner == m_owner) done(reply);
    });
}
void ShortcutLease::reconcile() {
    if (m_owner.isEmpty()) { for (const auto &binding : m_desired) Q_EMIT status(binding.action, "unavailable", {}); return; }
    QSet<QString> wanted; for (const auto &binding : m_desired) wanted.insert(binding.action);
    m_remove = (m_registered - wanted).values(); m_queue = m_desired; m_busy = true; next();
}
void ShortcutLease::next() {
    if (!m_remove.isEmpty()) {
        const auto id = m_remove.takeFirst();
        request("Unregister", {m_component, id}, [this, id](const auto &) { m_registered.remove(id); next(); }); return;
    }
    if (m_queue.isEmpty()) { m_busy = false; observe(); return; }
    const auto binding = m_queue.takeFirst();
    m_registered.insert(binding.action); // Unload must unregister in-flight claims too.
    request("Register", {m_component, binding.action, binding.description, texts(binding.keys), false, true}, [this, binding](const auto &reply) {
        if (!accepted(reply)) { m_errors[binding.action] = "conflict"; next(); return; }
        m_registered.insert(binding.action);
        if (!m_assign.remove(binding.action)) { next(); return; }
        request("SetShortcuts", {m_component, binding.action, texts(binding.keys)}, [this, binding](const auto &setReply) {
            if (!accepted(setReply)) m_errors[binding.action] = "conflict";
            next();
        });
    });
}
void ShortcutLease::bindingsChanged() { if (!m_busy) observe(); }
void ShortcutLease::observe() {
    if (m_observing) { m_observeAgain = true; return; }
    if (m_owner.isEmpty()) return;
    m_observing = true;
    request("ListBindings", {}, [this](const auto &reply) {
        m_observing = false;
        const auto rows = reply.type() == QDBusMessage::ReplyMessage && reply.signature() == "a{sv}" ? qdbus_cast<QVariantMap>(reply.arguments().first()) : QVariantMap{};
        m_ready.clear();
        for (const auto &binding : m_desired) {
            const auto row = rows.size() <= 4096 ? qdbus_cast<QVariantMap>(rows.value(m_component + QChar(0x1f) + binding.action)) : QVariantMap{};
            const auto actual = qdbus_cast<QStringList>(row.value("keys"));
            const auto reason = m_errors.contains(binding.action) ? m_errors.value(binding.action) : row.value("active").toBool() ? actual.isEmpty() ? QStringLiteral("unassigned") : QStringLiteral("ready") : QStringLiteral("unavailable");
            if (reason == "ready") m_ready.insert(binding.action);
            Q_EMIT status(binding.action, reason, actual);
        }
        if (m_observeAgain) { m_observeAgain = false; observe(); }
    });
}
void ShortcutLease::activatedOnBus(const QString &component, const QString &action, quint64 timestamp) {
    if (component == m_component && !m_busy && m_ready.contains(action)) Q_EMIT activated(action, timestamp);
}
} // namespace QindaQt::Services::Shortcuts
