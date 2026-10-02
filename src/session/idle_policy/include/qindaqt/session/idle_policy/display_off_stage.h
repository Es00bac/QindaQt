// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <functional>
#include <optional>

namespace QindaQt::Platform::Idle { class IdleObservation; }
namespace QindaQt::Power { class PowerClient; }

namespace QindaQt::Session::IdlePolicy {

struct DisplayOffPreferences final {
    bool enabled = false;
    int timeoutSeconds = 0;
    friend bool operator==(const DisplayOffPreferences &, const DisplayOffPreferences &) = default;
};

class DisplayPowerPort : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~DisplayPowerPort() override = default;
    virtual bool available() const = 0;
    virtual void requestOff() = 0;
    virtual void requestOn() = 0;
    // The final restore request is sent on the retained admitted FD before
    // teardown, even after the attachment's current liveness check has failed.
    virtual void restoreAndStop() = 0;
Q_SIGNALS:
    void availabilityChanged(bool available);
    void powerChanged(bool off);
    // Admission failure is separate from actual physical mode feedback.
    void requestFinished(bool admitted);
};

// Consumes only the display-off idle stage. The observation and power client
// are borrowed on this object's Qt thread and must outlive it. Preference
// lookup must return only a confirmed current Settings1/Power1 source choice.
// Losing attachment, DPMS, settings, or a live owner's receipt disarms the
// stage and best-effort restores power on; no output is disabled or removed.
class DisplayOffStage final : public QObject {
    Q_OBJECT
public:
    using Preferences = std::function<std::optional<DisplayOffPreferences>()>;
    DisplayOffStage(Platform::Idle::IdleObservation &idle, DisplayPowerPort &display,
                    Power::PowerClient &power, Preferences preferences,
                    QObject *parent = nullptr);
    void start();
    void refreshPreferences();
    void attachmentRevoked();
    void stop();
    bool displaysOffRequested() const noexcept { return m_offRequested; }
private:
    bool suppressed() const;
    void apply();
    void idleChanged();
    void powerChanged();
    void displayChanged(bool off);
    Platform::Idle::IdleObservation &m_idle;
    DisplayPowerPort &m_display;
    Power::PowerClient &m_power;
    Preferences m_preferences;
    std::optional<DisplayOffPreferences> m_current;
    int m_timeoutMilliseconds = 0;
    bool m_started = false;
    bool m_offRequested = false;
    bool m_cycleConsumed = false;
};

} // namespace QindaQt::Session::IdlePolicy
