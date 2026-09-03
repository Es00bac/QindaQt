// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/registrar/registrar_registry.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusContext>

namespace QindaQt::Shell::GlobalMenu::Registrar
{

class AppMenuRegistrarObject final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.AppMenu.Registrar")

public:
    explicit AppMenuRegistrarObject(RegistrarRegistry &registry, QObject *parent = nullptr);

public Q_SLOTS:
    Q_SCRIPTABLE void RegisterWindow(quint32 windowId, const QDBusObjectPath &menuObjectPath);
    Q_SCRIPTABLE void UnregisterWindow(quint32 windowId);
    Q_SCRIPTABLE void GetMenuForWindow(quint32 windowId, QString &service,
                                       QDBusObjectPath &menuObjectPath);
    Q_SCRIPTABLE RegistrarMenuList GetMenus();

Q_SIGNALS:
    Q_SCRIPTABLE void WindowRegistered(quint32 windowId, QString service,
                                       QDBusObjectPath menuObjectPath);
    Q_SCRIPTABLE void WindowUnregistered(quint32 windowId);

private:
    RegistrarRegistry &m_registry;
};

} // namespace QindaQt::Shell::GlobalMenu::Registrar
