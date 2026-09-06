// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QStringList>
#include <QVariantList>

namespace QindaQt::Apps::SettingsAppearance {

// Returns presentation maps ({name, path, previewUrl, value}) for readable PNG
// files in the ordered roots. Earlier roots win duplicate file names.
[[nodiscard]] QVariantList discoverBundledWallpapers(const QStringList &roots);

} // namespace QindaQt::Apps::SettingsAppearance
