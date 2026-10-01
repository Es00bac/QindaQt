// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/idle_policy/display_off_stage.h>

#include <qindaqt/platform/idle_observation/idle_observation.h>
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_protocol/power_types.h>

#include <algorithm>

namespace QindaQt::Session::IdlePolicy {
namespace {
constexpr int MaximumTimeoutSeconds = 14'400;
constexpr int MillisecondsPerSecond = 1'000;
}

DisplayOffStage::DisplayOffStage(Platform::Idle::IdleObservation &idle,
                                 DisplayPowerPort &display,
                                 Power::PowerClient &power,
                                 Preferences preferences, QObject *parent)
    : QObject(parent), m_idle(idle), m_display(display), m_power(power),
      m_preferences(std::move(preferences))
{
    connect(&m_idle, &Platform::Idle::IdleObservation::changed,
            this, &DisplayOffStage::idleChanged);
    connect(&m_display, &DisplayPowerPort::availabilityChanged,
            this, [this](bool) { apply(); });
    connect(&m_display, &DisplayPowerPort::powerChanged,
            this, &DisplayOffStage::displayChanged);
    connect(&m_power, &Power::PowerClient::snapshotChanged,
            this, &DisplayOffStage::powerChanged);
    connect(&m_power, &Power::PowerClient::stateChanged,
            this, &DisplayOffStage::powerChanged);
    connect(&m_power, &Power::PowerClient::idleInhibitorStateChanged,
            this, &DisplayOffStage::powerChanged);
}

void DisplayOffStage::start()
{
    if (m_started) return;
    m_started = true;
    refreshPreferences();
    m_idle.refresh();
    apply();
}

void DisplayOffStage::refreshPreferences()
{
    const auto next = m_preferences ? m_preferences() : std::nullopt;
    if (next == m_current) return;
    m_current = next;
    apply();
}

void DisplayOffStage::attachmentRevoked()
{
    if (!m_started) return;
    m_timeoutMilliseconds = 0;
    m_offRequested = false;
    m_idle.setTimeout(0);
    m_idle.revoke();
    // This last request uses only the already-connected FD. It cannot reconnect
    // to a replacement compositor at the old socket basename.
    m_display.restoreAndStop();
}

void DisplayOffStage::stop()
{
    if (!m_started) return;
    m_started = false;
    m_timeoutMilliseconds = 0;
    m_idle.setTimeout(0);
    m_idle.revoke();
    if (m_offRequested && m_display.available()) m_display.requestOn();
    m_offRequested = false;
}

bool DisplayOffStage::suppressed() const
{
    // Leases are destroyed with their owner. A missing Power1 owner therefore
    // has no live lease; a present owner with unknown state must fail closed.
    if (!m_power.owner().isEmpty() && !m_power.hasIdleInhibitorState()) return true;
    return m_power.hasIdleInhibitorState() &&
           m_power.activeIdleInhibitorScopes().testFlag(
               Power::IdleInhibitorScope::DisplayOff);
}

void DisplayOffStage::apply()
{
    if (!m_started) return;
    m_current = m_preferences ? m_preferences() : std::nullopt;
    const bool configured = m_current && m_current->enabled &&
        m_current->timeoutSeconds > 0 &&
        m_current->timeoutSeconds <= MaximumTimeoutSeconds;
    const bool armed = configured && m_display.available() && !suppressed();
    const int nextTimeout = armed ? m_current->timeoutSeconds * MillisecondsPerSecond : 0;
    if (nextTimeout != m_timeoutMilliseconds) {
        m_timeoutMilliseconds = nextTimeout;
        m_idle.setTimeout(m_timeoutMilliseconds);
    }
    if (!armed && m_offRequested) {
        m_offRequested = false;
        if (m_display.available()) m_display.requestOn();
    }
    if (armed && m_idle.available() && m_idle.idle()) idleChanged();
}

void DisplayOffStage::idleChanged()
{
    if (!m_started) return;
    if (!m_idle.available() || !m_idle.idle()) {
        if (m_offRequested) {
            m_offRequested = false;
            if (m_display.available()) m_display.requestOn();
        }
        return;
    }
    if (m_timeoutMilliseconds <= 0 || !m_display.available() || suppressed()) return;
    if (m_offRequested) return;
    m_offRequested = true;
    m_display.requestOff();
}

void DisplayOffStage::powerChanged()
{
    apply();
}

void DisplayOffStage::displayChanged(const bool off)
{
    if (!off && m_offRequested) m_offRequested = false;
}

} // namespace QindaQt::Session::IdlePolicy
