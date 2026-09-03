// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_wire.h>

#include <QtDBus/QDBusMetaType>

namespace QindaQt::Shell::GlobalMenu::DbusMenu
{

QDBusArgument &operator<<(QDBusArgument &argument, const LayoutItem &item)
{
    argument.beginStructure();
    argument << item.id << item.properties << item.children;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, LayoutItem &item)
{
    argument.beginStructure();
    argument >> item.id >> item.properties >> item.children;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PropertyEntry &entry)
{
    argument.beginStructure();
    argument << entry.id << entry.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PropertyEntry &entry)
{
    argument.beginStructure();
    argument >> entry.id >> entry.properties;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const RemovedPropertyEntry &entry)
{
    argument.beginStructure();
    argument << entry.id << entry.names;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, RemovedPropertyEntry &entry)
{
    argument.beginStructure();
    argument >> entry.id >> entry.names;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const EventEntry &entry)
{
    argument.beginStructure();
    argument << entry.id << entry.eventId << entry.data << entry.timestamp;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, EventEntry &entry)
{
    argument.beginStructure();
    argument >> entry.id >> entry.eventId >> entry.data >> entry.timestamp;
    argument.endStructure();
    return argument;
}

void registerDbusMenuWireTypes()
{
    // Test exporters and third-party adaptors may spell these namespace-local
    // aliases in moc metadata. Register both canonical C++ identities and the
    // stable alias spellings before any object is exported.
    qRegisterMetaType<LayoutItem>("DbusMenu::LayoutItem");
    qRegisterMetaType<LayoutItem>("LayoutItem");
    qRegisterMetaType<PropertyEntryList>("DbusMenu::PropertyEntryList");
    qRegisterMetaType<PropertyEntryList>("PropertyEntryList");
    qRegisterMetaType<RemovedPropertyEntryList>("DbusMenu::RemovedPropertyEntryList");
    qRegisterMetaType<RemovedPropertyEntryList>("RemovedPropertyEntryList");
    qRegisterMetaType<EventEntryList>("DbusMenu::EventEntryList");
    qRegisterMetaType<EventEntryList>("EventEntryList");
    qDBusRegisterMetaType<LayoutItem>();
    qDBusRegisterMetaType<PropertyEntry>();
    qDBusRegisterMetaType<RemovedPropertyEntry>();
    qDBusRegisterMetaType<EventEntry>();
    qDBusRegisterMetaType<PropertyEntryList>();
    qDBusRegisterMetaType<RemovedPropertyEntryList>();
    qDBusRegisterMetaType<EventEntryList>();
    qDBusRegisterMetaType<ShortcutList>();
}

} // namespace QindaQt::Shell::GlobalMenu::DbusMenu
