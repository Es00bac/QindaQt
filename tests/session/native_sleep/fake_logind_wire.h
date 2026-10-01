// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusMetaType>
namespace SleepTest {
struct UserWire { quint32 uid; QDBusObjectPath path; };
inline QDBusArgument &operator<<(QDBusArgument &a, const UserWire &u) {
  a.beginStructure(); a << u.uid << u.path; a.endStructure(); return a;
}
inline const QDBusArgument &operator>>(const QDBusArgument &a, UserWire &u) {
  a.beginStructure(); a >> u.uid >> u.path; a.endStructure(); return a;
}
}
Q_DECLARE_METATYPE(SleepTest::UserWire)
