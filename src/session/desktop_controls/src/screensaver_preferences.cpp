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
    static const QStringList savers{QStringLiteral("none"),
                                    QStringLiteral("qinda-patrol"),
                                    QStringLiteral("circuit-reef")};
    return savers;
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
    // Both savers place their own window on every output. Telemetry is off so
    // an unattended screen never renders live CPU, memory or network counters.
    if (saver == QLatin1String("qinda-patrol")) {
        return {QStringLiteral("--screensaver"), QStringLiteral("--no-metrics")};
    }
    if (saver == QLatin1String("circuit-reef")) {
        return {QStringLiteral("--all-screens"), QStringLiteral("--private")};
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
