// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusArgument>
#include <QDBusObjectPath>
namespace QindaQt::Tests {
struct LidUserWire { quint32 uid = 0; QDBusObjectPath path; };
inline QDBusArgument &operator<<(QDBusArgument &wire, const LidUserWire &user) {
    wire.beginStructure(); wire << user.uid << user.path; wire.endStructure(); return wire;
}
inline const QDBusArgument &operator>>(const QDBusArgument &wire, LidUserWire &user) {
    wire.beginStructure(); wire >> user.uid >> user.path; wire.endStructure(); return wire;
}
}
Q_DECLARE_METATYPE(QindaQt::Tests::LidUserWire)
