// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/applets/applet_manifest.h"
#include "qindaqt/profiles/layout_profile.h"

#include <QStringList>
#include <QVector>

namespace QindaQt::Apps::SettingsCustomize {

struct CustomizeCatalog final {
    QVector<Profiles::LayoutProfile> profiles;
    QVector<Applets::AppletManifest> manifests;
    QString error;
};

// Directories are low-to-high precedence. Loading is all-or-nothing: one
// malformed file disables the route instead of publishing a partial palette
// or profile set that would make placement acceptance misleading.
[[nodiscard]] CustomizeCatalog loadCustomizeCatalogs(
    const QStringList &profileDirectories,
    const QStringList &manifestDirectories);

} // namespace QindaQt::Apps::SettingsCustomize
