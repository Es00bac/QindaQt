// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/resident_power_service.h>
#include <qindaqt/services/power_service/unavailable_power_collaborators.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtDBus/QDBusConnection>

#include <memory>

using namespace QindaQt::Power;

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-power-service"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    QDBusConnection sessionConnection = QDBusConnection::sessionBus();
    // AGENT-GUARD: This activated process belongs to exactly the bus that
    // constructed it. Bus replacement must terminate the process; reconnecting
    // would expose stale collaborator/epoch state under a new authority
    // lineage.
    if (!sessionConnection.connect(
            QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
            QStringLiteral("org.freedesktop.DBus.Local"),
            QStringLiteral("Disconnected"), &application, SLOT(quit()))) {
        qCritical("Power1 could not bind constructing-bus lifetime");
        return 1;
    }

    // AGENT-NOTE: PB-1 deliberately injects the deterministic unavailable
    // collaborators; the resident process never contacts host UPower,
    // power-profiles-daemon, or logind. Later slices replace only this
    // composition.
    auto battery = std::make_unique<UnavailableBatteryCollaborator>();
    auto profiles = std::make_unique<UnavailableProfileCollaborator>();
    auto session = std::make_unique<UnavailableSessionCollaborator>();
    ResidentPowerService service(std::move(battery), std::move(profiles),
                                 std::move(session), sessionConnection);
    const PowerServiceStartStatus status = service.start();
    if (status != PowerServiceStartStatus::Started) {
        qCritical("Power1 startup failed with status %u",
                  static_cast<unsigned int>(status));
        return 1;
    }

    QObject::connect(&application, &QCoreApplication::aboutToQuit, &service,
                     &ResidentPowerService::stop);
    return QCoreApplication::exec();
}
