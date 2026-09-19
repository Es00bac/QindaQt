// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/screensaver_preferences.h"

#include <QtGlobal>

namespace QindaQt::Session::DesktopControls {

const QString &ScreensaverPreferences::noneToken()
{
    static const QString token = QStringLiteral("none");
    return token;
}

const QStringList &ScreensaverPreferences::knownSavers()
{
    // The token is the installed program name in every case. `starward` is
    // built from the starward-reimagined package, which is why the two names
    // differ there and nowhere else.
    static const QStringList savers{QStringLiteral("none"),
                                    QStringLiteral("qinda-patrol"),
                                    QStringLiteral("circuit-reef"),
                                    QStringLiteral("prism-circuit"),
                                    QStringLiteral("prism-brawl"),
                                    QStringLiteral("starward")};
    return savers;
}

bool ScreensaverPreferences::showsOnLockScreen(const QString &saver)
{
    return saver == QLatin1String("qinda-patrol")
        || saver == QLatin1String("circuit-reef");
}

bool ScreensaverPreferences::showsOnLockScreen() const
{
    return showsOnLockScreen(saver);
}

bool ScreensaverPreferences::enabled() const noexcept
{
    return !saver.isEmpty() && saver != noneToken();
}

QString ScreensaverPreferences::program() const
{
    return enabled() ? saver : QString{};
}

QStringList ScreensaverPreferences::arguments() const
{
    // AGENT-CONTRACT: every saver takes --screensaver -- natively in patrol,
    // as the documented alias in the other four -- and each covers every
    // connected output under it. The extra flag on each line turns off
    // something an unattended screen should not be doing: live system
    // counters, or sound that starts in an empty room.
    if (saver == QLatin1String("qinda-patrol")) {
        return {QStringLiteral("--screensaver"), QStringLiteral("--no-metrics")};
    }
    if (saver == QLatin1String("circuit-reef")) {
        return {QStringLiteral("--screensaver"), QStringLiteral("--private")};
    }
    if (saver == QLatin1String("prism-circuit")
        || saver == QLatin1String("prism-brawl")) {
        return {QStringLiteral("--screensaver"), QStringLiteral("--mute")};
    }
    if (saver == QLatin1String("starward")) {
        return {QStringLiteral("--screensaver")};
    }
    return {};
}

ScreensaverPreferences ScreensaverPreferences::fromPersisted(const QString &saver,
                                                             qint64 persistedMinutes)
{
    ScreensaverPreferences preferences;
    // An unknown token can never reach QProcess as a program name.
    preferences.saver = knownSavers().contains(saver) ? saver : noneToken();
    // All three arguments share one type so the qBound overload set stays
    // unambiguous.
    const qint64 maximum{maximumTimeoutMinutes()};
    preferences.minutes =
        static_cast<int>(qBound(qint64{1}, persistedMinutes, maximum));
    return preferences;
}

} // namespace QindaQt::Session::DesktopControls
