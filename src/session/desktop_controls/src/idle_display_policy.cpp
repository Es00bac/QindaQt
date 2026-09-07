// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/idle_display_policy.h"

#include <QtGlobal>

namespace QindaQt::Session::DesktopControls {
namespace {

constexpr int millisecondsPerMinute = 60'000;

} // namespace

IdleDisplayPolicy::IdleDisplayPolicy(IdlePreferencesProvider &preferences,
                                     IdleTracker &tracker, DpmsController &dpms,
                                     QObject *parent)
    : QObject(parent)
    , m_preferences(preferences)
    , m_tracker(tracker)
    , m_dpms(dpms)
{
    connect(&m_preferences, &IdlePreferencesProvider::preferencesChanged, this,
            &IdleDisplayPolicy::onPreferencesChanged);
    connect(&m_tracker, &IdleTracker::timeoutReached, this,
            &IdleDisplayPolicy::onTimeoutReached);
    connect(&m_tracker, &IdleTracker::resumingFromIdle, this,
            &IdleDisplayPolicy::onResumed);
    connect(&m_dpms, &DpmsController::displaysPowerChanged, this,
            &IdleDisplayPolicy::onDisplaysPowerChanged);
    connect(&m_dpms, &DpmsController::availabilityChanged, this,
            [this](bool available) {
                if (!available) {
                    return;
                }
                // AGENT-GUARD: an unavailable controller at start must not
                // permanently disarm the policy; recovery re-applies the
                // current preference even when Settings1 never re-sends it.
                applyPreferences(m_current);
            });
}

IdleDisplayPolicy::~IdleDisplayPolicy() = default;

void IdleDisplayPolicy::start()
{
    m_current = m_preferences.currentPreferences();
    applyPreferences(m_current);
    m_preferences.refresh();
}

void IdleDisplayPolicy::onPreferencesChanged(IdleDisplayPreferences preferences)
{
    if (preferences == m_current) {
        return;
    }
    m_current = preferences;
    applyPreferences(m_current);
}

void IdleDisplayPolicy::onTimeoutReached(int milliseconds)
{
    if (milliseconds != m_armedTimeoutMilliseconds || !m_current.enabled) {
        return;
    }
    if (!m_dpms.available()) {
        return;
    }
    m_displaysOffRequested = true;
    m_dpms.requestDisplaysOff();
}

void IdleDisplayPolicy::onResumed()
{
    // The compositor wakes outputs on input itself; requesting on here also
    // recovers from an off state reported late by the controller.
    if (m_displaysOffRequested) {
        m_displaysOffRequested = false;
        m_dpms.requestDisplaysOn();
    }
}

void IdleDisplayPolicy::onDisplaysPowerChanged(bool off)
{
    if (!off && m_displaysOffRequested) {
        m_displaysOffRequested = false;
    }
}

void IdleDisplayPolicy::applyPreferences(const IdleDisplayPreferences &preferences)
{
    if (m_armedTimeoutMilliseconds != 0) {
        m_tracker.disarmTimeout(m_armedTimeoutMilliseconds);
        m_armedTimeoutMilliseconds = 0;
    }
    if (!preferences.enabled || !m_dpms.available()) {
        if (m_displaysOffRequested) {
            m_displaysOffRequested = false;
            m_dpms.requestDisplaysOn();
        }
        return;
    }
    m_armedTimeoutMilliseconds = preferences.minutes * millisecondsPerMinute;
    m_tracker.armTimeout(m_armedTimeoutMilliseconds);
}

} // namespace QindaQt::Session::DesktopControls
