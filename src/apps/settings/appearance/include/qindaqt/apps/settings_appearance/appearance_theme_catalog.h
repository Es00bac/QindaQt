// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/themes/theme_spec.h"

#include <QStringList>
#include <QVector>

#include <optional>

namespace QindaQt::Apps::SettingsAppearance {

// Load every theme search directory in precedence order. Earlier directories
// win duplicate IDs; distinct themes from later directories remain visible.
// Any malformed discovered file fails the complete catalog closed.
[[nodiscard]] std::optional<QVector<Themes::ThemeSpec>>
loadAppearanceThemeDirectories(const QStringList &directories,
                               QString *error = nullptr);

} // namespace QindaQt::Apps::SettingsAppearance
