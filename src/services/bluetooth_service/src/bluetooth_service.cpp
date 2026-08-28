// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_service/bluetooth_service.h>
#include <qindaqt/services/bluetooth_model/bluetooth_model.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_dbus.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

#include <QtDBus/QDBusConnection>

namespace QindaQt::Bluetooth
{

BluetoothService::BluetoothService(QObject *parent)
    : QObject(parent)
    , m_model(std::make_unique<BluetoothModel>())
    , m_serviceName(QString::fromLatin1(kServiceName))
{
}

BluetoothService::~BluetoothService() = default;

bool BluetoothService::activateService(QDBusConnection &connection)
{
    registerDBusTypes();

    if (!connection.isConnected()) {
        return false;
    }

    if (!connection.registerService(m_serviceName)) {
        return false;
    }

    if (!connection.registerObject(QString::fromLatin1(kObjectPath), this,
                                   QDBusConnection::ExportAllContents)) {
        connection.unregisterService(m_serviceName);
        return false;
    }

    return true;
}

void BluetoothService::deactivateService()
{
    auto connection = QDBusConnection::sessionBus();
    connection.unregisterObject(QString::fromLatin1(kObjectPath));
    connection.unregisterService(m_serviceName);
}

Snapshot BluetoothService::getSnapshot() const
{
    if (!m_model) {
        Snapshot snapshot;
        snapshot.schemaVersion = 1;
        snapshot.epoch = 0;
        snapshot.revision = 0;
        snapshot.reasonCode = QStringLiteral("unavailable");
        snapshot.wireValid = true;
        return snapshot;
    }
    return m_model->currentSnapshot();
}

OperationResult BluetoothService::executeOperation(const OperationRequest &request)
{
    if (!m_model) {
        OperationResult result;
        result.kind = request.kind;
        result.status = OperationStatus::Rejected;
        result.reasonCode = QStringLiteral("unavailable");
        result.wireValid = true;
        return result;
    }
    return m_model->executeOperation(request);
}

} // namespace QindaQt::Bluetooth
