// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_theme_catalog.h"

#include "qindaqt/themes/theme_loader.h"

#include <QSet>

namespace QindaQt::Apps::SettingsAppearance {

std::optional<QVector<Themes::ThemeSpec>>
loadAppearanceThemeDirectories(const QStringList &directories, QString *error)
{
    QVector<Themes::ThemeSpec> themes;
    QSet<QString> ids;
    for (const QString &directory : directories) {
        const auto results = Themes::ThemeLoader::fromDirectory(directory);
        for (const auto &result : results) {
            if (!result.ok) {
                if (error != nullptr) {
                    *error = result.error;
                }
                return std::nullopt;
            }
            if (ids.contains(result.theme.id)) {
                continue;
            }
            ids.insert(result.theme.id);
            themes.append(result.theme);
        }
    }
    if (themes.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("no theme JSON files found in the search path");
        }
        return std::nullopt;
    }
    return themes;
}

} // namespace QindaQt::Apps::SettingsAppearance
