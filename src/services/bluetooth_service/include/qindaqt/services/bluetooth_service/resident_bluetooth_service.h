// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_model/bluetooth_model.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Bluetooth
{

class BluetoothServiceObject;

enum class ServiceStartStatus {
    Started,
    InvalidConnection,
    ObjectRegistrationFailed,
    NameAlreadyOwned,
    NameRegistrationFailed,
};

// Owns the service object/name and backend lifetime on the constructing Qt
// thread. stop() is idempotent, makes operations uncertain, releases D-Bus
// ownership before destruction, and drops every lease still held by vanished
// callers indirectly through model stop(). The named Qt connection is held by
// value and must remain registered until this object is destroyed.
class ResidentBluetoothService : public QObject
{
    Q_OBJECT

public:
    explicit ResidentBluetoothService(std::unique_ptr<AdapterBackend> backend,
                                      const QDBusConnection &connection,
                                      QString serviceName = {}, quint64 epochSeed = 0,
                                      QObject *parent = nullptr);
    ~ResidentBluetoothService() override;

    [[nodiscard]] ServiceStartStatus start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] BluetoothModel *model() noexcept;

private Q_SLOTS:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner,
                            const QString &newOwner);

private:
    std::unique_ptr<AdapterBackend> m_backend;
    std::unique_ptr<BluetoothModel> m_model;
    std::unique_ptr<BluetoothServiceObject> m_serviceObject;
    QDBusConnection m_connection;
    QString m_serviceName;
    bool m_objectRegistered = false;
    bool m_nameRegistered = false;
    bool m_ownerWatchInstalled = false;
};

} // namespace QindaQt::Bluetooth
