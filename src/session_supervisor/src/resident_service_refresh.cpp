// SPDX-License-Identifier: GPL-3.0-or-later
#include "resident_service_refresh.h"
#include "systemd_manager_port.h"
#include <QDebug>
#include <QtDBus/QDBusMessage>

namespace QindaQt::SessionSupervisor {
namespace {
constexpr int RestartTimeoutMilliseconds = 2'000;

// Sends one RestartUnit call along the resolved manager route. Returns false
// when the manager is unreachable or the call fails.
bool restartUnitOnManager(const SystemdManagerRoute &route, const QDBusConnection &bus,
                          const QString &unitName)
{
    const auto makeCall = [&unitName] {
        QDBusMessage restart = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.systemd1"), QStringLiteral("/org/freedesktop/systemd1"),
            QStringLiteral("org.freedesktop.systemd1.Manager"), QStringLiteral("RestartUnit"));
        restart.setArguments({unitName, QStringLiteral("replace")});
        return restart;
    };
    if (route.kind == SystemdManagerRoute::Kind::SessionBusName) {
        return bus.call(makeCall(), QDBus::Block, RestartTimeoutMilliseconds).type()
            != QDBusMessage::ErrorMessage;
    }
    if (route.kind == SystemdManagerRoute::Kind::Native) {
        return nativeRestartUnit(route.address, unitName, route.requiresBusHello);
    }
    return false;
}
}

QStringList residentServiceRefreshUnits()
{
    return {
        QStringLiteral("qindaqt-clipboard-host.service"),
        QStringLiteral("qindaqt-display-service.service"),
        QStringLiteral("plasma-xdg-desktop-portal-kde.service"),
        QStringLiteral("xdg-desktop-portal.service"),
    };
}

void refreshResidentServices(const QDBusConnection &bus,
                             const QStringList &unitNames,
                             const QString &systemdPrivateSocketPath)
{
    const SystemdManagerRoute route = resolveSystemdManagerRoute(bus, systemdPrivateSocketPath);
    for (const QString &unitName : unitNames) {
        if (unitName.isEmpty()) continue;
        if (!restartUnitOnManager(route, bus, unitName)) {
            qWarning("Could not restart resident session unit %s",
                    qUtf8Printable(unitName));
        }
    }
}

} // namespace QindaQt::SessionSupervisor
