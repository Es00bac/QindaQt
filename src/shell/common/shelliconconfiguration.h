// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

class QProcessEnvironment;

namespace QindaQt::Themes {
class ThemeCatalog;
}

namespace QindaQt::Shell {

struct ShellDataRoots {
    QString dataHome;
    QStringList dataDirectories;
};

// Shell-owned composition policy for the freedesktop icon module. ThemeCatalog
// remains responsible for visual tokens; this boundary consumes only the
// optional iconTheme presentation hint from the selected, validated catalog.
class ShellIconConfiguration final {
public:
    [[nodiscard]] static ShellDataRoots dataRoots(
        const QProcessEnvironment &environment, const QString &homeDirectory);
    [[nodiscard]] static bool selectedThemeName(
        const Themes::ThemeCatalog &themes, QString *themeName, QString *error);
};

} // namespace QindaQt::Shell
