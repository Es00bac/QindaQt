// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "idle_display_preferences.h"

#include <qindaqt/session/powerdevil_idle/powerdevil_idle_adapter.h>

#include <QObject>
#include <QtGlobal>

namespace QindaQt::Session::DesktopControls {

// Bridges Settings1 preference truth to the session-owned PowerDevil adapter.
// It owns no policy: PowerDevil owns the idle timer, inhibition, and DPMS.
// AGENT-CONTRACT: this GUI-thread object borrows both collaborators, coalesces
// preference changes while an asynchronous apply is pending, and keeps the
// latest value until the PowerDevil owner returns.
class PowerDevilIdlePreferencesBinding final : public QObject {
    Q_OBJECT

public:
    PowerDevilIdlePreferencesBinding(
        IdlePreferencesProvider &preferences,
        QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter &adapter,
        QObject *parent = nullptr);
    ~PowerDevilIdlePreferencesBinding() override;

    PowerDevilIdlePreferencesBinding(const PowerDevilIdlePreferencesBinding &) = delete;
    PowerDevilIdlePreferencesBinding &operator=(const PowerDevilIdlePreferencesBinding &) = delete;

    // Starts and stops the adapter and the preference refresh lifecycle.
    void start();
    void stop();

private Q_SLOTS:
    void onPreferencesChanged(IdleDisplayPreferences preferences);
    void onAvailabilityChanged();
    void onApplyFinished(bool success, const QString &error);

private:
    void queueDrain();
    void drain();
    [[nodiscard]] static int adapterMinutes(const IdleDisplayPreferences &preferences) noexcept;
    [[nodiscard]] static bool samePreferences(const IdleDisplayPreferences &left,
                                               const IdleDisplayPreferences &right) noexcept;

    IdlePreferencesProvider &m_preferences;
    QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter &m_adapter;
    IdleDisplayPreferences m_latest{};
    IdleDisplayPreferences m_acknowledged{};
    IdleDisplayPreferences m_inFlight{};
    bool m_started = false;
    bool m_haveLatest = false;
    bool m_haveAcknowledged = false;
    bool m_haveInFlight = false;
    quint64 m_preferenceRevision = 0;
    quint64 m_inFlightRevision = 0;
    bool m_blockedAfterFailure = false;
    bool m_waitingForAvailability = false;
    bool m_drainQueued = false;
};

} // namespace QindaQt::Session::DesktopControls
