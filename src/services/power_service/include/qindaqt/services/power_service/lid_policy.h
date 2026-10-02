// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/power_service/lid_handling_authority.h>
#include <qindaqt/services/power_service/power_service_coordinator.h>
#include <qindaqt/services/settings_client/settings_client.h>
namespace QindaQt::Services::SessionActions { class SessionActionsClient; }
namespace QindaQt::Power {
// Borrowed same-thread collaborators must outlive this policy. Dedicated
// SessionActions lifetime allows stop() to fence only this policy's pending
// Can callback. Once dispatched, uncertainty quarantines this policy lifetime.
class LidPolicy final : public QObject {
    Q_OBJECT
public:
    LidPolicy(PowerServiceCoordinator &power,
              Services::SettingsClient::SettingsClient &settings,
              LidHandlingAuthority &handling,
              Services::SessionActions::SessionActionsClient &actions,
              QObject *parent = nullptr);
    ~LidPolicy() override;
    static QStringList settingsKeys();
    void setNativeAuthority(bool admitted);
private:
    QString preferenceKey(QString *action = nullptr) const;
    QString lineage() const;
    bool actionAvailable(const QString &action) const;
    void observe();
    void reconcile();
    void dispatch(const QString &action);
    PowerServiceCoordinator &m_power;
    Services::SettingsClient::SettingsClient &m_settings;
    LidHandlingAuthority &m_handling;
    Services::SessionActions::SessionActionsClient &m_actions;
    QString m_lineage;
    bool m_native = false, m_scheduled = false, m_armed = false;
    bool m_pending = false, m_quarantined = false;
};
}
