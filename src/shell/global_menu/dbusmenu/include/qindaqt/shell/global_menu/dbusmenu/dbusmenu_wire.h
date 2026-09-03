// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusArgument>

namespace QindaQt::Shell::GlobalMenu::DbusMenu
{

inline constexpr auto kDbusMenuInterface = "com.canonical.dbusmenu";

// com.canonical.dbusmenu layout node, signature (ia{sv}av). Children are
// variants containing this same structure, exactly as the standard requires.
struct LayoutItem final {
    qint32 id = 0;
    QVariantMap properties;
    QVariantList children;
};

struct PropertyEntry final {
    qint32 id = 0;
    QVariantMap properties;
};

struct RemovedPropertyEntry final {
    qint32 id = 0;
    QStringList names;
};

using PropertyEntryList = QList<PropertyEntry>;
using RemovedPropertyEntryList = QList<RemovedPropertyEntry>;
using ShortcutList = QList<QStringList>;

QDBusArgument &operator<<(QDBusArgument &argument, const LayoutItem &item);
const QDBusArgument &operator>>(const QDBusArgument &argument, LayoutItem &item);
QDBusArgument &operator<<(QDBusArgument &argument, const PropertyEntry &entry);
const QDBusArgument &operator>>(const QDBusArgument &argument, PropertyEntry &entry);
QDBusArgument &operator<<(QDBusArgument &argument, const RemovedPropertyEntry &entry);
const QDBusArgument &operator>>(const QDBusArgument &argument, RemovedPropertyEntry &entry);
void registerDbusMenuWireTypes();

} // namespace QindaQt::Shell::GlobalMenu::DbusMenu

Q_DECLARE_METATYPE(QindaQt::Shell::GlobalMenu::DbusMenu::LayoutItem)
Q_DECLARE_METATYPE(QindaQt::Shell::GlobalMenu::DbusMenu::PropertyEntry)
Q_DECLARE_METATYPE(QindaQt::Shell::GlobalMenu::DbusMenu::RemovedPropertyEntry)
Q_DECLARE_METATYPE(QindaQt::Shell::GlobalMenu::DbusMenu::PropertyEntryList)
Q_DECLARE_METATYPE(QindaQt::Shell::GlobalMenu::DbusMenu::RemovedPropertyEntryList)
Q_DECLARE_METATYPE(QindaQt::Shell::GlobalMenu::DbusMenu::ShortcutList)
