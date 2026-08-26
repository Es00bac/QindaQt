// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applets/manifest_catalog.h"

#include "qindaqt/applets/manifest_loader.h"

#include <QDir>
#include <QSet>

#include <utility>

namespace QindaQt::Applets {

bool ManifestCatalog::loadDirectory(const QString &path, QString *error)
{
    const QDir directory(path);
    if (!directory.exists()) {
        if (error != nullptr) {
            *error = QStringLiteral("Applet manifest directory does not exist: %1").arg(path);
        }
        return false;
    }

    const QStringList fileNames =
        directory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    if (fileNames.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("Applet manifest directory is empty: %1").arg(path);
        }
        return false;
    }

    QVector<AppletManifest> loaded;
    loaded.reserve(fileNames.size());
    QSet<QString> identifiers;
    for (const QString &fileName : fileNames) {
        const ManifestLoadResult result = ManifestLoader::fromFile(directory.filePath(fileName));
        if (!result.ok) {
            if (error != nullptr) {
                *error = result.error;
            }
            return false;
        }
        if (identifiers.contains(result.manifest.id)) {
            if (error != nullptr) {
                *error = QStringLiteral("Duplicate applet manifest id: %1").arg(result.manifest.id);
            }
            return false;
        }
        identifiers.insert(result.manifest.id);
        loaded.append(result.manifest);
    }

    m_manifests = std::move(loaded);
    return true;
}

const QVector<AppletManifest> &ManifestCatalog::manifests() const
{
    return m_manifests;
}

const AppletManifest *ManifestCatalog::findById(QStringView id) const
{
    for (const AppletManifest &manifest : m_manifests) {
        if (manifest.id == id) {
            return &manifest;
        }
    }
    return nullptr;
}

} // namespace QindaQt::Applets
