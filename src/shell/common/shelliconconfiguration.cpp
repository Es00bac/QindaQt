// SPDX-License-Identifier: GPL-3.0-or-later
#include "shelliconconfiguration.h"

#include "qindaqt/themes/theme_catalog.h"

#include <QColor>
#include <QDir>
#include <QProcessEnvironment>
#include <QVariantMap>

namespace QindaQt::Shell {
namespace {

QStringList splitDataDirectories(const QString &value)
{
    const QString source = value.isEmpty()
        ? QStringLiteral("/usr/local/share:/usr/share")
        : value;
    QStringList directories;
    for (const QString &entry : source.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        const QString absolute = QDir::cleanPath(entry.trimmed());
        if (QDir::isAbsolutePath(absolute) && !directories.contains(absolute)) {
            directories.append(absolute);
        }
    }
    return directories;
}

QString defaultIconTheme(const QVariantMap &theme)
{
    const QVariantMap colors = theme.value(QStringLiteral("colors")).toMap();
    const QColor canvas(colors.value(QStringLiteral("canvas")).toString());
    if (canvas.isValid()) {
        return canvas.lightnessF() < 0.5 ? QStringLiteral("breeze-dark")
                                        : QStringLiteral("breeze");
    }
    const QString variant = theme.value(QStringLiteral("variant")).toString();
    return variant == QStringLiteral("light") ? QStringLiteral("breeze")
                                               : QStringLiteral("breeze-dark");
}

} // namespace

ShellDataRoots ShellIconConfiguration::dataRoots(
    const QProcessEnvironment &environment, const QString &homeDirectory)
{
    ShellDataRoots roots;
    const QString configuredHome = environment.value(QStringLiteral("XDG_DATA_HOME"));
    roots.dataHome = QDir::cleanPath(configuredHome);
    if (configuredHome.isEmpty() || !QDir::isAbsolutePath(roots.dataHome)) {
        roots.dataHome = QDir::cleanPath(
            QDir(homeDirectory).filePath(QStringLiteral(".local/share")));
    }
    roots.dataDirectories = splitDataDirectories(
        environment.value(QStringLiteral("XDG_DATA_DIRS")));
    return roots;
}

bool ShellIconConfiguration::selectedThemeName(
    const Themes::ThemeCatalog &themes, QString *themeName, QString *error)
{
    if (themeName == nullptr) {
        return false;
    }
    const QVariantMap selected = themes.current();
    const QString selectedId = selected.value(QStringLiteral("id")).toString();
    if (selectedId.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("icon configuration has no selected theme");
        }
        return false;
    }

    // AGENT-CONTRACT: ThemeLoader is the sole schema/parser authority. An
    // icon hint reaching ThemeCatalog has already passed its bounded grammar;
    // reopening the file here would create divergent validation and TOCTOU.
    const QString configured = selected.value(QStringLiteral("iconTheme")).toString();
    *themeName = configured.isEmpty() ? defaultIconTheme(selected) : configured;
    return true;
}

} // namespace QindaQt::Shell
