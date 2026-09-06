// SPDX-License-Identifier: GPL-3.0-or-later
#include "activation_environment.h"
#include <QDebug>
#include <QMap>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusMetaType>

namespace QindaQt::SessionSupervisor {
void publishActivationEnvironment(const QDBusConnection &bus,
                                  const QProcessEnvironment &environment)
{
    if (!bus.isConnected()) return;
    QMap<QString, QString> values;
    QStringList assignments;
    const QStringList names{QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                            QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("DISPLAY"),
                            QStringLiteral("XDG_RUNTIME_DIR"), QStringLiteral("XDG_SESSION_TYPE"),
                            QStringLiteral("XDG_CURRENT_DESKTOP"), QStringLiteral("XDG_SESSION_DESKTOP")};
    for (const QString &name : names) {
        if (!environment.value(name).isEmpty()) {
            values.insert(name, environment.value(name));
            assignments.append(name + QLatin1Char('=') + environment.value(name));
        }
    }
    qDBusRegisterMetaType<QMap<QString, QString>>();
    QDBusMessage broker = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("UpdateActivationEnvironment"));
    broker.setArguments({QVariant::fromValue(values)});
    // AGENT-CONTRACT: The session launches consumers only after both activation
    // paths know KWin's socket. Inherited user-manager values can name a retired
    // desktop; services must receive the current session's connection values.
    if (bus.call(broker, QDBus::Block, 2000).type() == QDBusMessage::ErrorMessage)
        qWarning("Could not update the D-Bus activation environment");
    QDBusMessage manager = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.systemd1"), QStringLiteral("/org/freedesktop/systemd1"),
        QStringLiteral("org.freedesktop.systemd1.Manager"), QStringLiteral("SetEnvironment"));
    manager.setArguments({assignments});
    if (bus.call(manager, QDBus::Block, 2000).type() == QDBusMessage::ErrorMessage)
        qWarning("Could not update the user service activation environment");
}
}
