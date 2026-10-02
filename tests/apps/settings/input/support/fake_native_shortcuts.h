// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusVirtualObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QVariantMap>
namespace QindaQt::Tests {
// Explicit native wire fixture; exercises client refusal and malformed replies,
// not compositor input/store policy, which is covered by the fork's real tests.
class FakeNativeShortcuts final : public QDBusVirtualObject {
public:
    QVariantMap rows;
    QStringList calls;
    bool refuse = false, malformed = false;
    bool publish(QDBusConnection bus) { return bus.registerService("org.qindaqt.Shortcuts1") && bus.registerVirtualObject("/org/qindaqt/Shortcuts1", this); }
    void add(const QString &component, const QString &action, const QStringList &keys, const QStringList &defaults = {}) {
        rows.insert(component + QChar(0x1f) + action, QVariantMap{{"component", component}, {"action", action}, {"componentLabel", "QindaQt Shell"}, {"description", "Show launcher"}, {"keys", keys}, {"defaults", defaults}, {"active", true}, {"repeat", false}});
    }
    QString introspect(const QString &) const override { return QStringLiteral("<interface name='org.qindaqt.Shortcuts1'/>"); }
    bool handleMessage(const QDBusMessage &call, const QDBusConnection &bus) override {
        calls.append(call.member()); const auto args = call.arguments(); const auto member = call.member();
        if (member == "ListBindings" && call.signature().isEmpty()) { bus.send(call.createReply(QVariantList{malformed ? QVariant(QStringLiteral("wrong")) : QVariant(rows)})); return true; }
        if (member == "Register" && call.signature() == "sssasbb") {
            if (!refuse) { add(args[0].toString(), args[1].toString(), qdbus_cast<QStringList>(args[3])); auto row = rows.value(args[0].toString() + QChar(0x1f) + args[1].toString()).toMap(); row["description"] = args[2]; rows[args[0].toString() + QChar(0x1f) + args[1].toString()] = row; }
            bus.send(call.createReply(QVariantList{!refuse, refuse ? QStringLiteral("Occupied by Lock session") : QString()})); return true;
        }
        if (member == "SetShortcuts" && call.signature() == "ssas") {
            const auto id = args[0].toString() + QChar(0x1f) + args[1].toString(); const bool accepted = !refuse && rows.contains(id);
            if (accepted) { auto row = rows.value(id).toMap(); row["keys"] = qdbus_cast<QStringList>(args[2]); rows[id] = row; }
            bus.send(call.createReply(QVariantList{accepted, accepted ? QString() : QStringLiteral("Occupied by Lock session")})); return true;
        }
        if (member == "Unregister" && call.signature() == "ss") { const bool removed = rows.remove(args[0].toString() + QChar(0x1f) + args[1].toString()); bus.send(call.createReply(QVariantList{removed})); return true; }
        bus.send(call.createErrorReply("org.freedesktop.DBus.Error.InvalidArgs", "Native shortcut contract mismatch")); return true;
    }
};
}
