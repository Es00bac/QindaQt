// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/resident_power_service.h>

#include "power_service_object_p.h"

#include <qindaqt/services/power_protocol/power_dbus.h>
#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtDBus/QDBusConnectionInterface>

#include <utility>

namespace QindaQt::Power {

ResidentPowerService::ResidentPowerService(
    std::unique_ptr<BatteryCollaborator> battery,
    std::unique_ptr<ProfileCollaborator> profiles,
    std::unique_ptr<SessionCollaborator> session, const QDBusConnection &connection,
    QString serviceName, QObject *parent)
    : QObject(parent)
    , m_battery(std::move(battery))
    , m_profiles(std::move(profiles))
    , m_session(std::move(session))
    , m_connection(connection)
    , m_serviceName(serviceName.isEmpty() ? QString::fromLatin1(kServiceName)
                                          : std::move(serviceName))
{
    Q_ASSERT(m_battery != nullptr && m_profiles != nullptr && m_session != nullptr);
    registerDBusTypes();
    m_coordinator =
        std::make_unique<PowerServiceCoordinator>(m_battery.get(), m_profiles.get(),
                                                  m_session.get());
    m_serviceObject =
        std::make_unique<PowerServiceObject>(m_coordinator.get(), m_connection);
}

ResidentPowerService::~ResidentPowerService()
{
    stop();
}

PowerServiceStartStatus ResidentPowerService::start()
{
    if (isRunning()) {
        return PowerServiceStartStatus::Started;
    }
    if (!m_connection.isConnected() || m_connection.interface() == nullptr) {
        return PowerServiceStartStatus::InvalidConnection;
    }

    const QString objectPath = QString::fromLatin1(kObjectPath);
    if (!m_connection.registerObject(objectPath, m_serviceObject.get(),
                                     QDBusConnection::ExportScriptableSlots
                                         | QDBusConnection::ExportScriptableSignals)) {
        return PowerServiceStartStatus::ObjectRegistrationFailed;
    }
    m_objectRegistered = true;

    if (!m_connection.registerService(m_serviceName)) {
        // AGENT-GUARD: Qt reports a foreign-owned name as a bare failure
        // without a NameExists D-Bus error (RequestName replies EXISTS as a
        // result code). Ask the bus who owns the name so an honest
        // NameAlreadyOwned never degrades into a generic failure.
        const QString currentOwner =
            m_connection.interface()->serviceOwner(m_serviceName).value();
        stop();
        return !currentOwner.isEmpty() ? PowerServiceStartStatus::NameAlreadyOwned
                                       : PowerServiceStartStatus::NameRegistrationFailed;
    }
    m_nameRegistered = true;
    m_coordinator->start();
    return PowerServiceStartStatus::Started;
}

void ResidentPowerService::stop()
{
    if (m_coordinator != nullptr) {
        m_coordinator->stop();
    }
    if (m_nameRegistered) {
        m_connection.unregisterService(m_serviceName);
        m_nameRegistered = false;
    }
    if (m_objectRegistered) {
        m_connection.unregisterObject(QString::fromLatin1(kObjectPath));
        m_objectRegistered = false;
    }
}

bool ResidentPowerService::isRunning() const noexcept
{
    return m_nameRegistered && m_objectRegistered;
}

PowerServiceCoordinator *ResidentPowerService::coordinator() noexcept
{
    return m_coordinator.get();
}

} // namespace QindaQt::Power
