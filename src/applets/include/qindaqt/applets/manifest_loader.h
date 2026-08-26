// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applets/applet_manifest.h"

#include <QByteArray>
#include <QString>

namespace QindaQt::Applets {

struct ManifestLoadResult final {
    bool ok = false;
    AppletManifest manifest;
    QString error;
};

class ManifestLoader final {
public:
    [[nodiscard]] static ManifestLoadResult fromFile(const QString &path);
    [[nodiscard]] static ManifestLoadResult fromJson(const QByteArray &json,
                                                     const QString &origin);
    [[nodiscard]] static QByteArray toJson(const AppletManifest &manifest);
};

} // namespace QindaQt::Applets
