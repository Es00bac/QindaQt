// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

#include <QtCore/QObject>
#include <QtCore/memory>

class QDBusConnection;

namespace QindaQt::Bluetooth
{

class BluetoothModel;

// The resident Bluetooth1 D-Bus service. This singleton object manages the
// BluetoothModel, publishes D-Bus snapshots, coordinates operation requests,
// and handles exact-owner semantics through the service activation boundary.
class BluetoothService : public QObject
{
    Q_OBJECT
public:
    explicit BluetoothService(QObject *parent = nullptr);
    ~BluetoothService() override;

    bool activateService(QDBusConnection &connection);
    void deactivateService();

    Snapshot getSnapshot() const;
    OperationResult executeOperation(const OperationRequest &request);

Q_SIGNALS:
    void changed(quint64 epoch, quint64 revision);

private:
    std::unique_ptr<BluetoothModel> m_model;
    QString m_serviceName;
};

} // namespace QindaQt::Bluetooth
