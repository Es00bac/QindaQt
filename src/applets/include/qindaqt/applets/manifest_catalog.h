// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applets/applet_manifest.h"

#include <QString>
#include <QStringView>
#include <QVector>

namespace QindaQt::Applets {

class ManifestCatalog final {
public:
    // Loading is atomic: on failure the previous catalog remains available.
    bool loadDirectory(const QString &path, QString *error = nullptr);

    [[nodiscard]] const QVector<AppletManifest> &manifests() const;

    // The returned pointer remains valid until the next successful load or
    // destruction of this catalog. Ownership stays with the catalog.
    [[nodiscard]] const AppletManifest *findById(QStringView id) const;

private:
    QVector<AppletManifest> m_manifests;
};

} // namespace QindaQt::Applets
