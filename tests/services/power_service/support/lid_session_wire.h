// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <optional>
namespace QindaQt::Tests {
// Public Session1/Sleep1 and ScreenSaver wire actor, never a machine action.
class LidSessionWire final : public QDBusVirtualObject {
public:
    explicit LidSessionWire(QDBusConnection bus) : m_bus(std::move(bus)) {}
    ~LidSessionWire() override { stop(); }
    bool start() {
        for (const auto &name : {QStringLiteral("org.qindaqt.Session1"), QStringLiteral("org.qindaqt.Sleep1"), QStringLiteral("org.freedesktop.ScreenSaver")})
            if (!m_bus.registerService(name)) return false;
        for (const auto &path : {QStringLiteral("/org/qindaqt/Session1"), QStringLiteral("/org/qindaqt/Sleep1"), QStringLiteral("/ScreenSaver")})
            if (!m_bus.registerVirtualObject(path, this)) return false;
        return true;
    }
    void stop() {
        for (const auto &name : {QStringLiteral("org.qindaqt.Session1"), QStringLiteral("org.qindaqt.Sleep1"), QStringLiteral("org.freedesktop.ScreenSaver")}) m_bus.unregisterService(name);
        for (const auto &path : {QStringLiteral("/org/qindaqt/Session1"), QStringLiteral("/org/qindaqt/Sleep1"), QStringLiteral("/ScreenSaver")}) m_bus.unregisterObject(path);
    }
    void replyCan() { if (delayedCan) { m_bus.send(delayedCan->createReply(true)); delayedCan.reset(); } }
    void replyAction() { if (delayedAction) { m_bus.send(delayedAction->createReply(true)); delayedAction.reset(); } }
    QString introspect(const QString &) const override { return QStringLiteral("<node/>"); }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &) override {
        const auto name = message.member();
        if (message.interface() == QStringLiteral("org.qindaqt.Sleep1")) {
            if (name == QStringLiteral("CanSuspend") || name == QStringLiteral("CanHibernate")) {
                ++canCalls;
                if (holdCan) delayedCan = message;
                else m_bus.send(message.createReply(true));
                return true;
            }
            if (name == QStringLiteral("Suspend") || name == QStringLiteral("Hibernate")) {
                actions.append(name);
                if (holdAction) delayedAction = message;
                else if (uncertain) m_bus.send(message.createErrorReply(QStringLiteral("org.qindaqt.Sleep1.Uncertain"), QStringLiteral("Private uncertainty")));
                else m_bus.send(message.createReply(true));
                return true;
            }
        }
        if (message.interface() == QStringLiteral("org.freedesktop.ScreenSaver")) {
            if (name == QStringLiteral("GetActive")) { m_bus.send(message.createReply(false)); return true; }
            if (name == QStringLiteral("Lock")) { actions.append(name); m_bus.send(message.createReply()); return true; }
        }
        if (message.interface() == QStringLiteral("org.qindaqt.Session1") && name == QStringLiteral("CanLogout")) {
            m_bus.send(message.createReply(false)); return true;
        }
        return false;
    }
    QStringList actions;
    int canCalls = 0;
    bool holdCan = false, holdAction = false, uncertain = false;
    std::optional<QDBusMessage> delayedCan, delayedAction;
private:
    QDBusConnection m_bus;
};
}
