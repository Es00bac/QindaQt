// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/idle_display_preferences.h"

#include <QtGlobal>

namespace QindaQt::Session::DesktopControls {

IdleDisplayPreferences IdleDisplayPreferences::fromMinutes(qint64 persistedMinutes)
{
    IdleDisplayPreferences preferences;
    if (persistedMinutes <= 0) {
        preferences.enabled = false;
        preferences.minutes = 0;
        return preferences;
    }
    preferences.enabled = true;
    preferences.minutes = static_cast<int>(
        qBound<qint64>(1, persistedMinutes,
                       static_cast<qint64>(maximumTimeoutMinutes())));
    return preferences;
}

qint64 IdleDisplayPreferences::toPersistedMinutes() const noexcept
{
    return enabled ? minutes : -1;
}

} // namespace QindaQt::Session::DesktopControls
