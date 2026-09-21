// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/screensaver_preferences.h"

#include "qindaqt/session/desktop_controls/screensaver_catalog.h"

#include <QtGlobal>

namespace QindaQt::Session::DesktopControls {

const QString &ScreensaverPreferences::noneToken()
{
    static const QString token = QStringLiteral("none");
    return token;
}

const QString &ScreensaverPreferences::blankToken()
{
    // "blank" is the built-in no-program choice: nothing runs while the
    // session is unlocked, and the locker's wallpaper plugin paints its plain
    // dark ground once locked (ADR-0226). It deliberately is not "none":
    // "none" hands the greeter back whatever wallpaper plugin it had before.
    static const QString token = QStringLiteral("blank");
    return token;
}

bool ScreensaverPreferences::enabled() const noexcept
{
    return !saver.isEmpty() && saver != noneToken() && saver != blankToken();
}

ScreensaverPreferences
ScreensaverPreferences::fromPersisted(const QString &saver,
                                      qint64 persistedMinutes,
                                      const ScreensaverCatalog &catalog)
{
    ScreensaverPreferences preferences;
    // An unknown token can never reach QProcess as a program name: only the
    // reserved tokens and a discovered catalog entry survive.
    if (saver == noneToken() || saver == blankToken()) {
        preferences.saver = saver;
    } else {
        preferences.saver = catalog.entry(saver).has_value() ? saver : noneToken();
    }
    // All three arguments share one type so the qBound overload set stays
    // unambiguous.
    const qint64 maximum{maximumTimeoutMinutes()};
    preferences.minutes =
        static_cast<int>(qBound(qint64{1}, persistedMinutes, maximum));
    return preferences;
}

} // namespace QindaQt::Session::DesktopControls
