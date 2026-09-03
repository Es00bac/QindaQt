// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/bluetooth_bluez_adapter/bluez_adapter_backend.h>
#include <qindaqt/services/bluetooth_bluez_adapter/bluez_backend_mode.h>
#include <qindaqt/services/bluetooth_model/deterministic_backend_factory.h>
#include <qindaqt/services/bluetooth_service/resident_bluetooth_service.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtDBus/QDBusConnection>

#include <memory>
#include <utility>

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

    // AGENT-CONTRACT: Backend selection is explicit and fails closed to
    // production (ADR-0056): only the exact value "deterministic" selects the
    // B0 empty backend. Production consumes org.bluez on the system bus
    // through an injected connection and tolerates BlueZ absence at startup.
    const QString requestedBackend = qEnvironmentVariable("QINDAQT_BLUETOOTH_BACKEND");
    std::unique_ptr<AdapterBackend> backend;
    if (resolveBluetoothBackendMode(requestedBackend)
        == BluetoothBackendMode::Deterministic) {
        backend = makeDeterministicAdapterBackend();
    } else {
        backend = std::make_unique<BluezAdapterBackend>(
            QDBusConnection::systemBus());
    }
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
