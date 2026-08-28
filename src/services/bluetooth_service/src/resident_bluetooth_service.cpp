// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/bluetooth_service/resident_bluetooth_service.h>

#include "bluetooth_service_object_p.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_dbus.h>

#include <QtDBus/QDBusConnectionInterface>

#include <utility>

namespace QindaQt::Bluetooth
{

ResidentBluetoothService::ResidentBluetoothService(std::unique_ptr<AdapterBackend> backend,
                                                   const QDBusConnection &connection,
                                                   QString serviceName,
                                                   const quint64 epochSeed, QObject *parent)
    : QObject(parent)
    , m_backend(std::move(backend))
    , m_connection(connection)
    , m_serviceName(serviceName.isEmpty() ? QString::fromLatin1(kServiceName)
                                          : std::move(serviceName))
{
    Q_ASSERT(m_backend != nullptr);
    registerDBusTypes();
    m_model = std::make_unique<BluetoothModel>(m_backend.get(), epochSeed);
    m_serviceObject =
        std::make_unique<BluetoothServiceObject>(m_model.get(), m_connection);
}

ResidentBluetoothService::~ResidentBluetoothService()
{
    stop();
}

ServiceStartStatus ResidentBluetoothService::start()
{
    if (isRunning()) {
        return ServiceStartStatus::Started;
    }
    if (!m_connection.isConnected() || m_connection.interface() == nullptr) {
        return ServiceStartStatus::InvalidConnection;
    }

    // AGENT-CONTRACT: Discovery leases are keyed by caller unique names, so
    // the service must observe caller loss on the same bus it serves. The
    // third connect argument is the D-Bus type signature of the match rule
    // (sss), not a name list. The subscription is installed once per running
    // lifetime and torn down on stop; a vanished lease holder drops its
    // leases so bounded discovery cannot leak after client death.
    if (!m_ownerWatchInstalled) {
        const bool watched = m_connection.connect(
            QString{}, QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"),
            this, SLOT(onNameOwnerChanged(QString,QString,QString)));
        if (!watched) {
            return ServiceStartStatus::InvalidConnection;
        }
        m_ownerWatchInstalled = true;
    }

    const QString objectPath = QString::fromLatin1(kObjectPath);
    if (!m_connection.registerObject(objectPath, m_serviceObject.get(),
                                     QDBusConnection::ExportScriptableSlots
                                         | QDBusConnection::ExportScriptableSignals)) {
        return ServiceStartStatus::ObjectRegistrationFailed;
    }
    m_objectRegistered = true;

    if (!m_connection.registerService(m_serviceName)) {
        const bool alreadyOwned = (m_connection.interface() != nullptr
                                   && m_connection.interface()->isServiceRegistered(m_serviceName))
            || m_connection.lastError().name()
                == QStringLiteral("org.freedesktop.DBus.Error.NameExists");
        stop();
        return alreadyOwned ? ServiceStartStatus::NameAlreadyOwned
                            : ServiceStartStatus::NameRegistrationFailed;
    }
    m_nameRegistered = true;
    m_model->start();
    return ServiceStartStatus::Started;
}

void ResidentBluetoothService::stop()
{
    if (m_model != nullptr) {
        m_model->stop();
    }
    if (m_nameRegistered) {
        m_connection.unregisterService(m_serviceName);
        m_nameRegistered = false;
    }
    if (m_objectRegistered) {
        m_connection.unregisterObject(QString::fromLatin1(kObjectPath));
        m_objectRegistered = false;
    }
    if (m_ownerWatchInstalled) {
        m_connection.disconnect(
            QString{}, QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"),
            this, SLOT(onNameOwnerChanged(QString,QString,QString)));
        m_ownerWatchInstalled = false;
    }
}

bool ResidentBluetoothService::isRunning() const noexcept
{
    return m_nameRegistered && m_objectRegistered;
}

BluetoothModel *ResidentBluetoothService::model() noexcept
{
    return m_model.get();
}

void ResidentBluetoothService::onNameOwnerChanged(const QString &name,
                                                  const QString &oldOwner,
                                                  const QString &newOwner)
{
    // AGENT-GUARD: Lease holders are unique D-Bus names. A unique name never
    // reappears, so an empty new owner with a nonempty old owner is an
    // unrecoverable client loss. A well-known name being relinquished without
    // replacement is not caller loss: its unique connection stays alive and
    // its leases must survive the alias release.
    if (!isRunning() || name.isEmpty() || oldOwner.isEmpty() || !newOwner.isEmpty()) {
        return;
    }
    if (!name.startsWith(QLatin1Char(':'))) {
        return;
    }
    m_model->ownerVanished(name);
}

} // namespace QindaQt::Bluetooth
