// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/portal/appearance_theme_catalog.h"

#include "qindaqt/themes/theme_loader.h"
#include "qindaqt/themes/theme_spec.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <utility>

namespace QindaQt::Services::Portal {
namespace {

constexpr qsizetype MaximumDirectories = 16;
constexpr qsizetype MaximumThemes = 128;
constexpr qint64 MaximumThemeDocumentBytes = 128 * 1024;
constexpr qsizetype MaximumIdentityBytes = 256;
constexpr qsizetype MaximumDisplayTextBytes = 1024;

void setError(QString *output, QString value)
{
    if (output != nullptr) {
        *output = std::move(value);
    }
}

bool boundedText(const QString &text, qsizetype maximumBytes)
{
    return !text.contains(QChar::Null) && text.toUtf8().size() <= maximumBytes;
}

} // namespace

std::optional<QVector<Themes::ThemeSpec>>
loadPortalAppearanceThemes(const QStringList &directories, QString *error)
{
    if (directories.isEmpty() || directories.size() > MaximumDirectories) {
        setError(error, QStringLiteral("portal theme search path count is invalid"));
        return std::nullopt;
    }
    QVector<Themes::ThemeSpec> themes;
    QSet<QString> ids;
    QSet<QString> seenDirectories;
    for (const QString &path : directories) {
        const QFileInfo directoryInfo(path);
        const QString absolute = directoryInfo.absoluteFilePath();
        if (!directoryInfo.isAbsolute() || seenDirectories.contains(absolute)) {
            setError(error,
                     QStringLiteral("portal theme search paths must be unique and absolute"));
            return std::nullopt;
        }
        seenDirectories.insert(absolute);
        if (!directoryInfo.exists()) {
            continue;
        }
        if (!directoryInfo.isDir()) {
            setError(error, QStringLiteral("portal theme search path is not a directory"));
            return std::nullopt;
        }
        const QDir directory(absolute);
        const QFileInfoList files = directory.entryInfoList(
            {QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo &file : files) {
            if (file.size() < 0 || file.size() > MaximumThemeDocumentBytes) {
                setError(error,
                         QStringLiteral("portal theme document exceeds the size bound"));
                return std::nullopt;
            }
            const auto loaded = Themes::ThemeLoader::fromFile(file.absoluteFilePath());
            if (!loaded.ok) {
                setError(error, loaded.error);
                return std::nullopt;
            }
            if (!boundedText(loaded.theme.id, MaximumIdentityBytes)
                || !boundedText(loaded.theme.name, MaximumDisplayTextBytes)
                || !boundedText(loaded.theme.variant, MaximumIdentityBytes)) {
                setError(error,
                         QStringLiteral("portal theme identity text exceeds its bound"));
                return std::nullopt;
            }
            if (ids.contains(loaded.theme.id)) {
                continue;
            }
            if (themes.size() >= MaximumThemes) {
                setError(error, QStringLiteral("portal theme catalog exceeds 128 entries"));
                return std::nullopt;
            }
            ids.insert(loaded.theme.id);
            themes.append(loaded.theme);
        }
    }
    if (themes.isEmpty()) {
        setError(error, QStringLiteral("portal theme catalog is empty"));
        return std::nullopt;
    }
    return themes;
}

} // namespace QindaQt::Services::Portal
