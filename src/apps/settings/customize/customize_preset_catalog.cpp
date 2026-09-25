// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_customize/customize_preset_catalog.h"

#include "qindaqt/profiles/profile_loader.h"

#include <QDir>
#include <QHash>

namespace QindaQt::Apps::SettingsCustomize {

PresetCatalog loadPresetCatalog(const QStringList &stockDirectories,
                                const QString &userDirectory)
{
    PresetCatalog result;
    QHash<QString, qsizetype> indexes;
    for (const QString &path : stockDirectories) {
        if (!QDir(path).exists()) {
            continue;
        }
        for (const auto &item : Profiles::ProfileLoader::fromDirectory(path)) {
            if (!item.ok) {
                return {{}, item.error.diagnostic()};
            }
            const auto existing = indexes.constFind(item.profile.id);
            if (existing == indexes.cend()) {
                indexes.insert(item.profile.id, result.presets.size());
                result.presets.append({item.profile, item.profile, true, false});
            } else {
                // A later installed directory overrides an earlier one.
                result.presets[*existing].profile = item.profile;
                result.presets[*existing].original = item.profile;
            }
        }
    }
    if (!userDirectory.isEmpty() && QDir(userDirectory).exists()) {
        for (const auto &item : Profiles::ProfileLoader::fromDirectory(userDirectory)) {
            if (!item.ok) {
                return {{}, item.error.diagnostic()};
            }
            const auto existing = indexes.constFind(item.profile.id);
            if (existing == indexes.cend()) {
                indexes.insert(item.profile.id, result.presets.size());
                result.presets.append({item.profile, std::nullopt, false, true});
            } else {
                // The user copy wins; `original` keeps the installed one.
                result.presets[*existing].profile = item.profile;
                result.presets[*existing].userCopy = true;
            }
        }
    }
    if (result.presets.isEmpty()) {
        result.error = QStringLiteral("No validated layout profiles were found");
    }
    return result;
}

} // namespace QindaQt::Apps::SettingsCustomize
