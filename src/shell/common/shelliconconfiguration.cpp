// SPDX-License-Identifier: GPL-3.0-or-later
#include "shelliconconfiguration.h"

#include "qindaqt/themes/theme_catalog.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QVariantMap>

namespace QindaQt::Shell {
namespace {

constexpr qint64 maxThemeBytes = 256 * 1024;

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

bool isValidIconThemeName(const QString &name)
{
    static const QRegularExpression allowed(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$"));
    return allowed.match(name).hasMatch() && !name.contains(QStringLiteral(".."));
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
    roots.dataHome = QDir::cleanPath(environment.value(
        QStringLiteral("XDG_DATA_HOME"),
        QDir(homeDirectory).filePath(QStringLiteral(".local/share"))));
    if (!QDir::isAbsolutePath(roots.dataHome)) {
        roots.dataHome.clear();
    }
    roots.dataDirectories = splitDataDirectories(
        environment.value(QStringLiteral("XDG_DATA_DIRS")));
    return roots;
}

bool ShellIconConfiguration::selectedThemeName(
    const Themes::ThemeCatalog &themes, const QString &themeDirectory,
    QString *themeName, QString *error)
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

    QDir directory(themeDirectory);
    const QStringList files = directory.entryList(
        {QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name);
    for (const QString &fileName : files) {
        QFile file(directory.filePath(fileName));
        const QFileInfo info(file);
        if (info.size() > maxThemeBytes || !file.open(QIODevice::ReadOnly)) {
            continue;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            file.read(maxThemeBytes + 1), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }
        const QJsonObject object = document.object();
        if (object.value(QStringLiteral("id")).toString() != selectedId) {
            continue;
        }
        const QJsonValue configured = object.value(QStringLiteral("iconTheme"));
        if (configured.isUndefined()) {
            *themeName = defaultIconTheme(selected);
            return true;
        }
        if (!configured.isString() ||
            !isValidIconThemeName(configured.toString())) {
            if (error != nullptr) {
                *error = QStringLiteral(
                    "theme %1 has an invalid iconTheme value").arg(selectedId);
            }
            return false;
        }
        *themeName = configured.toString();
        return true;
    }
    if (error != nullptr) {
        *error = QStringLiteral("selected theme %1 is missing from %2")
                     .arg(selectedId, themeDirectory);
    }
    return false;
}

} // namespace QindaQt::Shell
