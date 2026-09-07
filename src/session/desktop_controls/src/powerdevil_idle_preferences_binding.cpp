// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/session/desktop_controls/powerdevil_idle_preferences_binding.h"

#include <QMetaObject>

namespace QindaQt::Session::DesktopControls {

PowerDevilIdlePreferencesBinding::PowerDevilIdlePreferencesBinding(
    IdlePreferencesProvider &preferences,
    QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter &adapter,
    QObject *parent)
    : QObject(parent)
    , m_preferences(preferences)
    , m_adapter(adapter)
{
    connect(&m_preferences, &IdlePreferencesProvider::preferencesChanged, this,
            &PowerDevilIdlePreferencesBinding::onPreferencesChanged);
    connect(&m_adapter,
            &QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter::availabilityChanged,
            this, &PowerDevilIdlePreferencesBinding::onAvailabilityChanged);
    connect(&m_adapter,
            &QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter::applyFinished,
            this, &PowerDevilIdlePreferencesBinding::onApplyFinished);
}

PowerDevilIdlePreferencesBinding::~PowerDevilIdlePreferencesBinding()
{
    stop();
}

void PowerDevilIdlePreferencesBinding::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    m_haveLatest = true;
    m_latest = m_preferences.currentPreferences();
    m_blockedAfterFailure = false;
    m_waitingForAvailability = false;

    m_adapter.start();
    m_preferences.refresh();
    queueDrain();
}

void PowerDevilIdlePreferencesBinding::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    m_haveLatest = false;
    m_blockedAfterFailure = false;
    m_waitingForAvailability = false;
    m_adapter.stop();
}

void PowerDevilIdlePreferencesBinding::onPreferencesChanged(
    const IdleDisplayPreferences preferences)
{
    if (!m_started) {
        return;
    }
    m_latest = preferences;
    m_haveLatest = true;
    // A new preference is an explicit retry after a previous config or reload
    // failure. Repeated failure signals alone never create a retry loop.
    m_blockedAfterFailure = false;
    m_waitingForAvailability = false;
    queueDrain();
}

void PowerDevilIdlePreferencesBinding::onAvailabilityChanged()
{
    if (!m_started) {
        return;
    }
    if (!m_adapter.available()) {
        m_waitingForAvailability = true;
        return;
    }

    // Owner return is an explicit retry boundary for an unavailable apply.
    m_blockedAfterFailure = false;
    m_waitingForAvailability = false;
    queueDrain();
}

void PowerDevilIdlePreferencesBinding::onApplyFinished(const bool success,
                                                       const QString &error)
{
    if (!m_started) {
        return;
    }
    if (success) {
        m_blockedAfterFailure = false;
        queueDrain();
        return;
    }

    const bool ownerUnavailable = !m_adapter.available()
        || error == QLatin1String("powerdevil-unavailable")
        || error == QLatin1String("powerdevil-owner-lost");
    if (ownerUnavailable) {
        m_waitingForAvailability = true;
        m_blockedAfterFailure = false;
        return;
    }

    if (error == QLatin1String("powerdevil-owner-replaced")) {
        // The replacement owner is already available. Queue the latest value
        // after ownerChanged() returns so its recovery request cannot be
        // re-entered or leave the adapter busy.
        m_waitingForAvailability = false;
        m_blockedAfterFailure = false;
        queueDrain();
        return;
    }

    // Config and refresh failures are surfaced once. A later preference
    // change or owner return clears this block; failure itself never retries.
    m_blockedAfterFailure = true;
}

void PowerDevilIdlePreferencesBinding::queueDrain()
{
    if (!m_started || m_drainQueued) {
        return;
    }
    m_drainQueued = true;
    QMetaObject::invokeMethod(this, [this] {
        m_drainQueued = false;
        drain();
    }, Qt::QueuedConnection);
}

void PowerDevilIdlePreferencesBinding::drain()
{
    if (!m_started || !m_haveLatest || m_blockedAfterFailure
        || m_adapter.applying()) {
        return;
    }
    if (!m_adapter.available()) {
        m_waitingForAvailability = true;
        return;
    }

    const int minutes = adapterMinutes(m_latest);
    if (m_adapter.enabled() == m_latest.enabled
        && m_adapter.minutes() == minutes) {
        m_waitingForAvailability = false;
        return;
    }

    m_waitingForAvailability = false;
    if (!m_adapter.apply(m_latest.enabled, minutes)) {
        const bool ownerUnavailable = !m_adapter.available();
        m_waitingForAvailability = ownerUnavailable;
        m_blockedAfterFailure = !ownerUnavailable;
    }
}

int PowerDevilIdlePreferencesBinding::adapterMinutes(
    const IdleDisplayPreferences &preferences) noexcept
{
    // IdleDisplayPreferences uses minutes=0 for disabled, while PowerDevil
    // requires a positive stored timeout even when its enabled flag is false.
    return preferences.enabled ? preferences.minutes
                                : IdleDisplayPreferences::defaultTimeoutMinutes();
}

} // namespace QindaQt::Session::DesktopControls
