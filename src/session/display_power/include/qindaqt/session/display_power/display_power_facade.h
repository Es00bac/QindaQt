// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/session/idle_policy/display_off_stage.h>
#include <functional>
#include <optional>
namespace QindaQt::Power { class PowerClient; }
namespace QindaQt::Session::NativeLockRuntime { class Runtime; }
namespace QindaQt::Session::DisplayPower {
class ScopedDisplayPower;
// Same-thread, borrowed collaborators outlive this facade. Idle and persistent
// lid episodes are independent causes in one compositor; guarded submission
// reuses Runtime's actual Protected gate, never method-reported lock acceptance.
class DisplayPowerFacade final : public IdlePolicy::DisplayPowerPort {
    Q_OBJECT
public:
    DisplayPowerFacade(ScopedDisplayPower &display, Power::PowerClient &power,
                      NativeLockRuntime::Runtime &lock,
                      std::function<std::optional<bool>()> lockBeforeOff,
                      QObject *parent = nullptr);
    ~DisplayPowerFacade() override;
    bool available() const override;
    void requestOff() override;
    void requestOn() override;
    void restoreAndStop() override;
    void refreshPreferences();
    bool canScreenOff() const;
    bool callerAllowed(const QString &actualCaller) const;
    bool screenOffEpisodeHeld() const { return !m_lidId.isEmpty(); }
    bool requestScreenOff(const QString &actualCaller, quint64 powerEpoch, const QString &id);
    bool releaseScreenOff(const QString &actualCaller, const QString &id);
Q_SIGNALS:
    void screenOffFinished(const QString &id, bool admitted);
    void screenOffEnded(const QString &id);
private:
    bool submit(const QString &cause);
    void releaseCause(const QString &cause);
    void finished(const QString &cause, bool admitted);
    ScopedDisplayPower &m_display;
    Power::PowerClient &m_power;
    NativeLockRuntime::Runtime &m_lock;
    std::function<std::optional<bool>()> m_preferences;
    QString m_idleCause, m_lidCause, m_lidCaller, m_lidId, m_guarding;
    quint64 m_lidEpoch = 0, m_guardSerial = 0;
    bool m_lidDelivered = false;
};
}
