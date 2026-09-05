// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/registrar/registrar_registry.h>

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtDBus/QDBusContext>

namespace QindaQt::Shell::GlobalMenu::Registrar
{

class AppMenuRegistrarObject final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.AppMenu.Registrar")

public:
    explicit AppMenuRegistrarObject(RegistrarRegistry &registry, QObject *parent = nullptr);
    void setHostedMenu(const QString &providerUniqueName,
                       const QString &objectPath, bool hosted);
    void clearHostedMenus();

public Q_SLOTS:
    Q_SCRIPTABLE void RegisterWindow(quint32 windowId, const QDBusObjectPath &menuObjectPath);
    Q_SCRIPTABLE void UnregisterWindow(quint32 windowId);
    Q_SCRIPTABLE void GetMenuForWindow(quint32 windowId, QString &service,
                                       QDBusObjectPath &menuObjectPath);
    Q_SCRIPTABLE RegistrarMenuList GetMenus();
    Q_SCRIPTABLE bool IsMenuHosted(const QString &providerUniqueName,
                                   const QDBusObjectPath &menuObjectPath) const;

Q_SIGNALS:
    Q_SCRIPTABLE void WindowRegistered(quint32 windowId, QString service,
                                       QDBusObjectPath menuObjectPath);
    Q_SCRIPTABLE void WindowUnregistered(quint32 windowId);
    Q_SCRIPTABLE void MenuHostedChanged(QString providerUniqueName,
                                        QDBusObjectPath menuObjectPath,
                                        bool hosted);

private:
    RegistrarRegistry &m_registry;
    QHash<QString, QSet<QString>> m_hostedMenus;
};

} // namespace QindaQt::Shell::GlobalMenu::Registrar
