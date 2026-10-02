// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusVirtualObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QTimer>
#include <optional>
namespace QindaQt::Tests {
// Standard producer-wire fault injection only. Normal runtime rows use the
// real resident host and authenticated public presentation action boundary.
class CriticalNotificationFault final : public QDBusVirtualObject {
public:
    explicit CriticalNotificationFault(QDBusConnection bus) : m_bus(std::move(bus)) {}
    ~CriticalNotificationFault() override {
        m_bus.unregisterService(QStringLiteral("org.freedesktop.Notifications"));
        m_bus.unregisterObject(QStringLiteral("/org/freedesktop/Notifications"));
    }
    bool start() {
        return m_bus.registerVirtualObject(QStringLiteral("/org/freedesktop/Notifications"), this)
            && m_bus.registerService(QStringLiteral("org.freedesktop.Notifications"));
    }
    void replyNotify() {
        if (pending) { m_bus.send(pending->createReply(QVariant::fromValue(quint32(41)))); pending.reset(); }
    }
    QString introspect(const QString &) const override { return QStringLiteral("<node/>"); }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &) override {
        if (message.interface() != QStringLiteral("org.freedesktop.Notifications")) return false;
        if (message.member() == QStringLiteral("GetCapabilities")) {
            m_bus.send(message.createReply(QStringList{QStringLiteral("actions")})); return true;
        }
        if (message.member() == QStringLiteral("Notify") && message.arguments().size() == 8) {
            ++notifyCount; pending = message; retained = true;
            expiryMilliseconds = message.arguments().last().toInt();
            QTimer::singleShot(expiryMilliseconds, this, [this] { retained = false; });
            return true;
        }
        if (message.member() == QStringLiteral("CloseNotification")) {
            closedIds.append(message.arguments().first().toUInt()); retained = false;
            m_bus.send(message.createReply()); return true;
        }
        return false;
    }
    int notifyCount = 0, expiryMilliseconds = 0;
    bool retained = false;
    QList<quint32> closedIds;
    std::optional<QDBusMessage> pending;
private:
    QDBusConnection m_bus;
};
}
