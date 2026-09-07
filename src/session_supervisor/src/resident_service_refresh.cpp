// SPDX-License-Identifier: GPL-3.0-or-later
#include "resident_service_refresh.h"
#include <QDebug>
#include <QtDBus/QDBusMessage>

namespace QindaQt::SessionSupervisor {
namespace {
constexpr int RestartTimeoutMilliseconds = 2'000;
}

QStringList residentWaylandServiceUnits()
{
    return {
        QStringLiteral("qindaqt-clipboard-host.service"),
        QStringLiteral("qindaqt-display-service.service"),
    };
}

void refreshResidentWaylandServices(const QDBusConnection &bus,
                                    const QStringList &unitNames)
{
    if (!bus.isConnected()) return;
    for (const QString &unitName : unitNames) {
        if (unitName.isEmpty()) continue;
        QDBusMessage restart = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.systemd1"), QStringLiteral("/org/freedesktop/systemd1"),
            QStringLiteral("org.freedesktop.systemd1.Manager"), QStringLiteral("RestartUnit"));
        restart.setArguments({unitName, QStringLiteral("replace")});
        if (bus.call(restart, QDBus::Block, RestartTimeoutMilliseconds).type()
            == QDBusMessage::ErrorMessage) {
            qWarning("Could not restart resident session unit %s",
                    qUtf8Printable(unitName));
        }
    }
}

} // namespace QindaQt::SessionSupervisor
