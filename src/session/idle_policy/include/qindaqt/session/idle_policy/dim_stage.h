// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/session/idle_policy/shared_preferences.h>
#include <QObject>
#include <functional>
namespace QindaQt::Platform::Idle { class IdleObservation; }
namespace QindaQt::Power { class PowerClient; }
namespace QindaQt::Session::IdlePolicy {
// Owns only the brightness change made for this idle episode. Restoration
// requires same handle/epoch and unchanged observed dim value; external/manual
// brightness changes are never overwritten. Borrowed collaborators share thread.
class DimStage final : public QObject {
    Q_OBJECT
public:
    using Preferences = std::function<std::optional<SharedIdlePreferences>()>;
    DimStage(Platform::Idle::IdleObservation &idle, Power::PowerClient &power,
             Preferences preferences, QObject *parent = nullptr);
    ~DimStage() override;
    void start();
    void stop();
    void refreshPreferences();
private:
    void apply();
    void restore();
    void clearOwned();
    bool suppressed() const;
    Platform::Idle::IdleObservation &m_idle;
    Power::PowerClient &m_power;
    Preferences m_preferences;
    std::optional<SharedIdlePreferences> m_current;
    QString m_owner;
    Power::Handle m_device;
    quint64 m_epoch = 0, m_operation = 0, m_minRevision = 0;
    quint32 m_before = 0, m_dim = 0;
    int m_timeout = 0;
    bool m_started = false, m_consumed = false, m_restoring = false, m_restoreWanted = false, m_quarantined = false;
};
}
