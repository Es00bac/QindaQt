// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/power_service/critical_notification.h>
#include <qindaqt/services/power_service/power_service_coordinator.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <QElapsedTimer>
#include <QTimer>
namespace QindaQt::Services::SessionActions { class SessionActionsClient; }
namespace QindaQt::Power {
// Same-thread borrowed collaborators outlive this object. The action client is
// dedicated to this policy: stop() revokes its predispatch work on cancellation.
// A started attempt consumes its critical episode until authenticated recovery;
// uncertain mutation quarantines dispatch for this object's entire lifetime.
class CriticalBatteryPolicy final : public QObject {
    Q_OBJECT
public:
    CriticalBatteryPolicy(PowerServiceCoordinator &power,
        Services::SettingsClient::SettingsClient &settings,
        CriticalNotification &notification,
        Services::SessionActions::SessionActionsClient &actions,
        QObject *parent = nullptr);
    ~CriticalBatteryPolicy() override;
    static QStringList settingsKeys();
    void setNativeAuthority(bool admitted);
private:
    enum class Phase { Idle, Announcing, Counting, Closing, Action, Spent };
    void schedule();
    void reconcile();
    void withdraw();
    QString preferenceKey() const;
    bool actionAvailable(const QString &action) const;
    bool currentAttempt() const;
    void dispatch();
    PowerServiceCoordinator &m_power;
    Services::SettingsClient::SettingsClient &m_settings;
    CriticalNotification &m_notification;
    Services::SessionActions::SessionActionsClient &m_actions;
    QTimer m_tick;
    QElapsedTimer m_elapsed;
    QString m_preferenceKey, m_action;
    quint64 m_powerEpoch = 0;
    int m_seconds = 0, m_remaining = 0;
    Phase m_phase = Phase::Idle;
    bool m_authority = false, m_attempted = false, m_quarantined = false;
    bool m_scheduled = false, m_actionPending = false;
};
}
