// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessiondefaults.h"

#include <QDir>
#include <QSettings>
#include <QVariant>

namespace QindaQt::Session {
namespace {

void seedMissing(QSettings &settings, const QString &key,
                 const QVariant &value)
{
    if (!settings.contains(key)) {
        settings.setValue(key, value);
    }
}

} // namespace

bool SessionDefaults::ensure(const QString &configHome, QString *error)
{
    if (configHome.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("the configuration home is empty");
        }
        return false;
    }

    QDir directory(configHome);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = QStringLiteral("could not create configuration directory '%1'")
                         .arg(configHome);
        }
        return false;
    }

    QSettings kwin(directory.filePath(QStringLiteral("kwinrc")), QSettings::IniFormat);
    kwin.beginGroup(QStringLiteral("org.kde.kdecoration2"));
    // AGENT-CONTRACT: `org.qindaqt` is the installed KDecoration3 module
    // name. First-run defaults select the coherent QindaQt desktop without
    // turning each launcher invocation into an edit of the user's choices.
    seedMissing(kwin, QStringLiteral("library"), QStringLiteral("org.qindaqt"));
    kwin.endGroup();

    kwin.beginGroup(QStringLiteral("Windows"));
    // AGENT-GUARD: An ordinary drag remains a floating-window move. The
    // explicit Meta+Shift path owns QindaQt edge/corner docking; KWin's absent
    // defaults otherwise add a second, conflicting pointer interpretation.
    seedMissing(kwin, QStringLiteral("ElectricBorderTiling"), false);
    seedMissing(kwin, QStringLiteral("ElectricBorderMaximize"), false);
    kwin.endGroup();

    kwin.beginGroup(QStringLiteral("TabBox"));
    // KWin retains switch/focus authority and supplies the native model. The
    // installed package changes presentation only and consumes the
    // compositor's one-representative-per-container skipSwitcher policy.
    seedMissing(kwin, QStringLiteral("LayoutName"), QStringLiteral("qindaqt"));
    kwin.endGroup();

    kwin.sync();
    if (kwin.status() != QSettings::NoError) {
        if (error) {
            *error = QStringLiteral("could not persist QindaQt session defaults in '%1'")
                         .arg(kwin.fileName());
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Session
