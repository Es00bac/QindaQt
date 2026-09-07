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
    ++m_preferenceRevision;
    m_haveAcknowledged = false;
    m_haveInFlight = false;
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
    m_haveAcknowledged = false;
    m_haveInFlight = false;
    ++m_preferenceRevision;
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
    ++m_preferenceRevision;
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
        m_haveAcknowledged = false;
        return;
    }

    // Owner return is an explicit retry boundary for an unavailable apply.
    m_haveAcknowledged = false;
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
    const bool hadInFlight = m_haveInFlight;
    const quint64 inFlightRevision = m_inFlightRevision;
    const IdleDisplayPreferences inFlight = m_inFlight;
    m_haveInFlight = false;

    if (success) {
        if (hadInFlight) {
            m_acknowledged = inFlight;
            m_haveAcknowledged = true;
        }
        m_blockedAfterFailure = false;
        queueDrain();
        return;
    }

    // Adapter fields are optimistic: apply() updates them before the daemon
    // confirms refreshStatus. A failed request must invalidate that state so a
    // later explicit retry cannot be suppressed by an equality comparison.
    m_haveAcknowledged = false;

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

    if (hadInFlight && m_preferenceRevision > inFlightRevision) {
        // A newer preference arrived while the failed request was in flight.
        // One queued attempt for that newer revision is valid; only that
        // attempt's own failure may establish the retry block.
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
    if (m_haveAcknowledged && samePreferences(m_acknowledged, m_latest)) {
        m_waitingForAvailability = false;
        return;
    }

    m_waitingForAvailability = false;
    m_inFlight = m_latest;
    m_inFlightRevision = m_preferenceRevision;
    m_haveInFlight = true;
    if (!m_adapter.apply(m_latest.enabled, minutes)) {
        // apply() normally emits applyFinished synchronously for rejection;
        // retain a safe fallback for an adapter that only returns false.
        if (m_haveInFlight) {
            m_haveInFlight = false;
            m_haveAcknowledged = false;
            const bool ownerUnavailable = !m_adapter.available();
            m_waitingForAvailability = ownerUnavailable;
            m_blockedAfterFailure = !ownerUnavailable;
        }
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

bool PowerDevilIdlePreferencesBinding::samePreferences(
    const IdleDisplayPreferences &left,
    const IdleDisplayPreferences &right) noexcept
{
    return left.enabled == right.enabled && left.minutes == right.minutes;
}

} // namespace QindaQt::Session::DesktopControls
