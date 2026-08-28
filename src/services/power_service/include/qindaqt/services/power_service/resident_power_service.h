// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_collaborators.h>
#include <qindaqt/services/power_service/power_service_coordinator.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Power {

class PowerServiceObject;

enum class PowerServiceStartStatus {
    Started,
    InvalidConnection,
    ObjectRegistrationFailed,
    NameAlreadyOwned,
    NameRegistrationFailed,
};

// AGENT-CONTRACT: Owns the service object/name and the three collaborator
// lifetimes on the constructing Qt thread. The named Qt connection must
// remain registered until this object is destroyed. start() is idempotent;
// stop() is idempotent, makes every pending operation Uncertain, and releases
// D-Bus ownership before destruction. A NameAlreadyOwned start leaves no
// object or name registered.
class ResidentPowerService : public QObject
{
    Q_OBJECT

public:
    ResidentPowerService(std::unique_ptr<BatteryCollaborator> battery,
                         std::unique_ptr<ProfileCollaborator> profiles,
                         std::unique_ptr<SessionCollaborator> session,
                         const QDBusConnection &connection, QString serviceName = {},
                         QObject *parent = nullptr);
    ~ResidentPowerService() override;

    [[nodiscard]] PowerServiceStartStatus start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] PowerServiceCoordinator *coordinator() noexcept;

private:
    std::unique_ptr<BatteryCollaborator> m_battery;
    std::unique_ptr<ProfileCollaborator> m_profiles;
    std::unique_ptr<SessionCollaborator> m_session;
    std::unique_ptr<PowerServiceCoordinator> m_coordinator;
    std::unique_ptr<PowerServiceObject> m_serviceObject;
    QDBusConnection m_connection;
    QString m_serviceName;
    bool m_objectRegistered = false;
    bool m_nameRegistered = false;
};

} // namespace QindaQt::Power
