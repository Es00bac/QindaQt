// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "dpms_controller.h"
#include "idle_display_preferences.h"
#include "idle_tracker.h"

#include <QObject>

namespace QindaQt::Session::DesktopControls {

// Configurable idle display-off: when the user has been idle for the
// configured minutes, request DPMS off; when activity resumes, request DPMS
// on. This is display power only — it never locks the session and never
// touches the separate screen-lock preference. A disabled preference or an
// unavailable DPMS controller keeps displays on with no timeout armed.
class IdleDisplayPolicy final : public QObject {
    Q_OBJECT

public:
    IdleDisplayPolicy(IdlePreferencesProvider &preferences, IdleTracker &tracker,
                      DpmsController &dpms, QObject *parent = nullptr);
    ~IdleDisplayPolicy() override;

    IdleDisplayPolicy(const IdleDisplayPolicy &) = delete;
    IdleDisplayPolicy &operator=(const IdleDisplayPolicy &) = delete;

    void start();

    [[nodiscard]] bool displaysOffRequested() const noexcept { return m_displaysOffRequested; }
    [[nodiscard]] int armedTimeoutMilliseconds() const noexcept { return m_armedTimeoutMilliseconds; }

private Q_SLOTS:
    void onPreferencesChanged(
        QindaQt::Session::DesktopControls::IdleDisplayPreferences preferences);
    void onTimeoutReached(int milliseconds);
    void onResumed();
    void onDisplaysPowerChanged(bool off);

private:
    void applyPreferences(const IdleDisplayPreferences &preferences);

    IdlePreferencesProvider &m_preferences;
    IdleTracker &m_tracker;
    DpmsController &m_dpms;
    IdleDisplayPreferences m_current;
    int m_armedTimeoutMilliseconds = 0;
    bool m_displaysOffRequested = false;
};

} // namespace QindaQt::Session::DesktopControls
