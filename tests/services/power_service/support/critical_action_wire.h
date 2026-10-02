// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusVirtualObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <optional>
namespace QindaQt::Tests {
// Wire-faithful protected Sleep1/Session1 and noninteractive power-off fixture.
// Delayed capability/action replies exercise cancellation without real sleep.
class CriticalActionWire final : public QDBusVirtualObject {
public:
    explicit CriticalActionWire(QDBusConnection bus) : m_bus(std::move(bus)) {}
    ~CriticalActionWire() override { stop(); }
    bool start() {
        for (const auto &name : {QStringLiteral("org.qindaqt.Sleep1"), QStringLiteral("org.qindaqt.Session1"),
                                QStringLiteral("org.freedesktop.login1")})
            if (!m_bus.registerService(name)) return false;
        for (const auto &path : {QStringLiteral("/org/qindaqt/Sleep1"), QStringLiteral("/org/qindaqt/Session1"),
                                QStringLiteral("/org/freedesktop/login1")})
            if (!m_bus.registerVirtualObject(path, this)) return false;
        return true;
    }
    void stop() {
        for (const auto &name : {QStringLiteral("org.qindaqt.Sleep1"), QStringLiteral("org.qindaqt.Session1"),
                                QStringLiteral("org.freedesktop.login1")}) m_bus.unregisterService(name);
        for (const auto &path : {QStringLiteral("/org/qindaqt/Sleep1"), QStringLiteral("/org/qindaqt/Session1"),
                                QStringLiteral("/org/freedesktop/login1")}) m_bus.unregisterObject(path);
    }
    void replyCapability() {
        if (delayedCan) { m_bus.send(delayedCan->createReply(true)); delayedCan.reset(); }
    }
    void replyAction() {
        if (delayedAction) { m_bus.send(delayedAction->createReply(true)); delayedAction.reset(); }
    }
    QString introspect(const QString &) const override { return QStringLiteral("<node/>"); }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &) override {
        const auto name = message.member();
        if (message.interface() == QStringLiteral("org.qindaqt.Sleep1")) {
            if (name == QStringLiteral("CanSuspend") || name == QStringLiteral("CanHibernate")) {
                ++canCount;
                if (holdCan) delayedCan = message;
                else m_bus.send(message.createReply(true));
                return true;
            }
            if (name == QStringLiteral("Suspend") || name == QStringLiteral("Hibernate")) {
                actions.append(name);
                if (holdAction) delayedAction = message;
                else if (uncertain) m_bus.send(message.createErrorReply(QStringLiteral("org.qindaqt.Sleep1.Uncertain"),
                    QStringLiteral("Private injected uncertainty; not replayed")));
                else m_bus.send(message.createReply(true));
                return true;
            }
        }
        if (message.interface() == QStringLiteral("org.freedesktop.login1.Manager")) {
            if (name == QStringLiteral("CanPowerOff") || name == QStringLiteral("CanReboot")) {
                m_bus.send(message.createReply(QStringLiteral("yes"))); return true;
            }
            if (name == QStringLiteral("PowerOff")) {
                actions.append(name);
                nonInteractive = message.signature() == QStringLiteral("b") && !message.arguments().first().toBool();
                m_bus.send(message.createReply()); return true;
            }
            if (name == QStringLiteral("Suspend") || name == QStringLiteral("Hibernate")) {
                ++directSleepCalls;
                m_bus.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                    QStringLiteral("Direct sleep is forbidden"))); return true;
            }
        }
        if (message.interface() == QStringLiteral("org.qindaqt.Session1") && name == QStringLiteral("CanLogout")) {
            m_bus.send(message.createReply(false)); return true;
        }
        return false;
    }
    QStringList actions;
    int canCount = 0, directSleepCalls = 0;
    bool holdCan = false, holdAction = false, uncertain = false, nonInteractive = false;
    std::optional<QDBusMessage> delayedCan, delayedAction;
private:
    QDBusConnection m_bus;
};
}
