// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/registrar/registrar_wire.h>

#include <QtDBus/QDBusMetaType>

namespace QindaQt::Shell::GlobalMenu::Registrar
{

QDBusArgument &operator<<(QDBusArgument &argument, const RegistrarMenu &menu)
{
    argument.beginStructure();
    argument << menu.windowId << menu.service << menu.objectPath;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, RegistrarMenu &menu)
{
    argument.beginStructure();
    argument >> menu.windowId >> menu.service >> menu.objectPath;
    argument.endStructure();
    return argument;
}

void registerRegistrarWireTypes()
{
    qDBusRegisterMetaType<RegistrarMenu>();
    qDBusRegisterMetaType<RegistrarMenuList>();
}

} // namespace QindaQt::Shell::GlobalMenu::Registrar
