// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessiondefaults.h"

#include <QDir>
#include <QSettings>

namespace QindaQt::Session {

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
    if (!kwin.contains(QStringLiteral("library"))) {
        // AGENT-CONTRACT: `org.qindaqt` is the installed KDecoration3 module
        // name. This first-run seed selects the Qinda macOS behavior without
        // turning each launcher invocation into an edit of the user's choice.
        kwin.setValue(QStringLiteral("library"), QStringLiteral("org.qindaqt"));
    }
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
