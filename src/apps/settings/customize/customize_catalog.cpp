// SPDX-License-Identifier: GPL-3.0-or-later
#include "customize_catalog.h"

#include "qindaqt/applets/manifest_loader.h"
#include "qindaqt/profiles/profile_loader.h"

#include <QDir>
#include <QHash>

namespace QindaQt::Apps::SettingsCustomize {

CustomizeCatalog loadCustomizeCatalogs(const QStringList &profileDirectories,
                                       const QStringList &manifestDirectories)
{
    CustomizeCatalog result;
    QHash<QString, qsizetype> profileIndexes;
    for (const QString &path : profileDirectories) {
        const QDir directory(path);
        if (!directory.exists()) {
            continue;
        }
        const auto loaded = Profiles::ProfileLoader::fromDirectory(path);
        for (const auto &item : loaded) {
            if (!item.ok) {
                result.error = item.error.diagnostic();
                return result;
            }
            const auto existing = profileIndexes.constFind(item.profile.id);
            if (existing == profileIndexes.cend()) {
                profileIndexes.insert(item.profile.id, result.profiles.size());
                result.profiles.append(item.profile);
            } else {
                result.profiles[*existing] = item.profile;
            }
        }
    }

    QHash<QString, qsizetype> manifestIndexes;
    for (const QString &path : manifestDirectories) {
        const QDir directory(path);
        if (!directory.exists()) {
            continue;
        }
        const QStringList files = directory.entryList(
            {QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QString &file : files) {
            const auto loaded = Applets::ManifestLoader::fromFile(
                directory.filePath(file));
            if (!loaded.ok) {
                result.error = loaded.error;
                return result;
            }
            const auto existing = manifestIndexes.constFind(loaded.manifest.id);
            if (existing == manifestIndexes.cend()) {
                manifestIndexes.insert(loaded.manifest.id,
                                       result.manifests.size());
                result.manifests.append(loaded.manifest);
            } else {
                result.manifests[*existing] = loaded.manifest;
            }
        }
    }
    if (result.profiles.isEmpty()) {
        result.error = QStringLiteral("No validated layout profiles were found");
    } else if (result.manifests.isEmpty()) {
        result.error = QStringLiteral("No validated applet manifests were found");
    }
    return result;
}

} // namespace QindaQt::Apps::SettingsCustomize
