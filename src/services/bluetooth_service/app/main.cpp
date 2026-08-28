// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/bluetooth_model/deterministic_backend_factory.h>
#include <qindaqt/services/bluetooth_service/resident_bluetooth_service.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtDBus/QDBusConnection>

#include <memory>

using namespace QindaQt::Bluetooth;

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-bluetooth-service"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    QDBusConnection sessionConnection = QDBusConnection::sessionBus();
    // AGENT-GUARD: This activated process belongs to exactly the bus that
    // constructed it. Bus replacement must terminate the process; reconnecting
    // would expose stale backend/epoch state under a new authority lineage.
    if (!sessionConnection.connect(
            QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
            QStringLiteral("org.freedesktop.DBus.Local"),
            QStringLiteral("Disconnected"), &application, SLOT(quit()))) {
        qCritical("Bluetooth1 could not bind constructing-bus lifetime");
        return 1;
    }

    // B0 platform adapter: deterministic and initially empty, so an activated
    // process without the BluezQt runtime lane publishes a truthful
    // Unavailable/no-adapter snapshot instead of fabricated inventory. See
    // ADR-0026 for the replacement boundary.
    auto backend = makeDeterministicAdapterBackend();
    ResidentBluetoothService service(std::move(backend), sessionConnection);
    const ServiceStartStatus status = service.start();
    if (status != ServiceStartStatus::Started) {
        qCritical("Bluetooth1 startup failed with status %u",
                  static_cast<unsigned int>(status));
        return 1;
    }

    QObject::connect(&application, &QCoreApplication::aboutToQuit, &service,
                     &ResidentBluetoothService::stop);
    return QCoreApplication::exec();
}
