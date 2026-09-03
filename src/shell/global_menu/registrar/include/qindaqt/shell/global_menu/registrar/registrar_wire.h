// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QtGlobal>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusObjectPath>

namespace QindaQt::Shell::GlobalMenu::Registrar
{

inline constexpr auto kRegistrarServiceName = "com.canonical.AppMenu.Registrar";
inline constexpr auto kRegistrarObjectPath = "/com/canonical/AppMenu/Registrar";
inline constexpr auto kRegistrarInterface = "com.canonical.AppMenu.Registrar";

// Wire value returned by GetMenus. Internal owner/generation evidence is
// deliberately absent from the compatibility protocol.
struct RegistrarMenu final {
    quint32 windowId = 0;
    QString service;
    QDBusObjectPath objectPath;

    bool operator==(const RegistrarMenu &) const = default;
};

using RegistrarMenuList = QList<RegistrarMenu>;

QDBusArgument &operator<<(QDBusArgument &argument, const RegistrarMenu &menu);
const QDBusArgument &operator>>(const QDBusArgument &argument, RegistrarMenu &menu);
void registerRegistrarWireTypes();

} // namespace QindaQt::Shell::GlobalMenu::Registrar

Q_DECLARE_METATYPE(QindaQt::Shell::GlobalMenu::Registrar::RegistrarMenu)
Q_DECLARE_METATYPE(QindaQt::Shell::GlobalMenu::Registrar::RegistrarMenuList)
