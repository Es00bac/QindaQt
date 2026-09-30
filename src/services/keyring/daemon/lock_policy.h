// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "idle_observer.h"
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/session_lock_state/session_lock_state.h>
#include <functional>
namespace qindaqt::keyring::service {
class LockObservation : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QindaQt::Services::SessionLockState::LockState state() const=0;
Q_SIGNALS:
    void changed();
};
// One thread. All collaborators/callback captures outlive this policy.
// Uses last confirmed same-owner Settings1 policy; no user-file read, optimistic
// persistence or unlock action. Enabled policy with uncertain observation locks
// fail-closed and reports degraded; it never fabricates an idle event.
class KeyringLockPolicy final : public QObject {
    Q_OBJECT
public:
    KeyringLockPolicy(QindaQt::Services::SettingsClient::SettingsClient &,
                      LockObservation &,IdleObservation &,std::function<void()> lockAll,
                      QObject *parent=nullptr);
    void enforce();
    QVariantMap status() const;
Q_SIGNALS:
    void changed();
private:
    void snapshot();
    QindaQt::Services::SettingsClient::SettingsClient &settings_;
    LockObservation &screen_;
    IdleObservation &idle_;
    std::function<void()> lockAll_;
    bool lockOnScreen_=false,enforcing_=false;
    int idleMinutes_=0;
};
}
