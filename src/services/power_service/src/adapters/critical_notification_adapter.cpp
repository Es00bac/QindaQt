// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/power_service/adapters/critical_notification_adapter.h>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QUuid>
namespace QindaQt::Power::Upstream {
namespace {
const QString Service = QStringLiteral("org.freedesktop.Notifications");
const QString Path = QStringLiteral("/org/freedesktop/Notifications");
QDBusMessage method(const QString &owner, const QString &name, const QVariantList &args = {}) {
    auto call = QDBusMessage::createMethodCall(owner, Path, Service, name);
    call.setArguments(args);
    return call;
}
}
CriticalNotificationAdapter::CriticalNotificationAdapter(QDBusConnection bus, QObject *parent)
    : CriticalNotification(parent), m_bus(std::move(bus)) {
    // Subscribe before owner resolution. Every callback checks the current
    // unique owner; queued owner payloads never supply admission.
    m_subscribed = m_bus.connect({}, Path, Service, QStringLiteral("ActionInvoked"),
        this, SLOT(actionInvoked(QDBusMessage)))
        && m_bus.connect({}, Path, Service, QStringLiteral("NotificationClosed"),
        this, SLOT(notificationClosed(QDBusMessage)));
    m_watcher = new QDBusServiceWatcher(Service, m_bus,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this] {
        if (m_wanted && currentOwner() != m_owner) reject();
    });
}
CriticalNotificationAdapter::~CriticalNotificationAdapter() {
    // No synchronous drain or fabricated ID on shutdown. Positive expiry also
    // bounds a notification whose initial reply never provided an owned ID.
    if (m_id != 0 && !m_owner.isEmpty())
        m_bus.send(method(m_owner, QStringLiteral("CloseNotification"), {m_id}));
}
QString CriticalNotificationAdapter::currentOwner() const {
    if (!m_bus.isConnected() || !m_bus.interface()) return {};
    m_bus.interface()->setTimeout(250);
    const QDBusReply<QString> owner = m_bus.interface()->serviceOwner(Service);
    return owner.isValid() ? owner.value() : QString{};
}
void CriticalNotificationAdapter::show(const QString &action, int seconds) {
    if (!m_subscribed || m_pending || m_closing || m_id != 0 || seconds < 5 || seconds > 300) {
        Q_EMIT failed(); return;
    }
    m_owner = currentOwner();
    if (m_owner.isEmpty()) { Q_EMIT failed(); return; }
    m_action = action;
    m_seconds = seconds;
    m_cancelKey = QStringLiteral("cancel.") + QUuid::createUuid().toString(QUuid::Id128);
    m_wanted = true;
    m_closeRequested = false;
    m_pending = true;
    const auto owner = m_owner;
    auto *watcher = new QDBusPendingCallWatcher(
        m_bus.asyncCall(method(owner, QStringLiteral("GetCapabilities")), 750), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, owner] {
        const QDBusPendingReply<QStringList> reply = *watcher;
        watcher->deleteLater();
        m_pending = false;
        if (!m_wanted) { close(); return; }
        if (reply.isError() || !reply.value().contains(QStringLiteral("actions"))
            || currentOwner() != owner) { reject(); return; }
        notify(m_seconds, true);
    });
}
void CriticalNotificationAdapter::update(int seconds) {
    if (!m_wanted || m_pending || m_closing || m_id == 0) return;
    notify(seconds, false);
}
void CriticalNotificationAdapter::notify(int seconds, bool first) {
    if (currentOwner() != m_owner) { reject(); return; }
    const auto owner = m_owner;
    const auto replaces = m_id;
    m_pending = true;
    const QString verb = m_action == QStringLiteral("suspend") ? tr("suspend")
        : m_action == QStringLiteral("hibernate") ? tr("hibernate") : tr("power off");
    auto call = method(owner, QStringLiteral("Notify"), {
        QStringLiteral("QindaQt"), replaces, QStringLiteral("battery-caution"),
        tr("Battery critically low"), tr("This computer will %1 in %2 seconds.").arg(verb).arg(seconds),
        QStringList{m_cancelKey, tr("Cancel")},
        QVariantMap{{QStringLiteral("urgency"), QVariant::fromValue(quint8(2))},
                    {QStringLiteral("desktop-entry"), QStringLiteral("qindaqt-power-service")}},
        (seconds + 3) * 1000});
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 1000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, watcher, owner, first] {
            const QDBusPendingReply<quint32> reply = *watcher;
            watcher->deleteLater();
            m_pending = false;
            if (reply.isError() || reply.value() == 0) { reject(); return; }
            // A late typed reply still authorizes cleanup on its original
            // unique owner, never against a replacement host's numeric IDs.
            m_id = reply.value();
            if (!m_wanted || currentOwner() != owner || m_closeRequested) { close(); return; }
            if (first) Q_EMIT shown();
        });
}
void CriticalNotificationAdapter::reject() {
    m_wanted = false;
    Q_EMIT failed();
    close();
}
void CriticalNotificationAdapter::close() {
    m_wanted = false;
    m_closeRequested = true;
    if (m_pending || m_closing) return;
    if (m_id == 0) { Q_EMIT closed(); return; }
    const auto id = m_id;
    m_id = 0;
    m_closing = true;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(
        method(m_owner, QStringLiteral("CloseNotification"), {id}), 1000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        m_closing = false;
        if (reply.type() == QDBusMessage::ReplyMessage && reply.signature().isEmpty()) Q_EMIT closed();
        else Q_EMIT failed();
    });
}
void CriticalNotificationAdapter::actionInvoked(const QDBusMessage &message) {
    if (!m_wanted || message.service() != m_owner || currentOwner() != m_owner
        || message.signature() != QStringLiteral("us") || message.arguments().size() != 2
        || message.arguments().first().toUInt() != m_id
        || message.arguments().last().toString() != m_cancelKey) return;
    m_wanted = false;
    Q_EMIT cancelled();
    close();
}
void CriticalNotificationAdapter::notificationClosed(const QDBusMessage &message) {
    if (!m_wanted || m_closing || message.service() != m_owner || currentOwner() != m_owner
        || message.signature() != QStringLiteral("uu") || message.arguments().size() != 2
        || message.arguments().first().toUInt() != m_id) return;
    m_id = 0;
    m_wanted = false;
    Q_EMIT cancelled();
}
}
