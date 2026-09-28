// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/themes/theme_spec.h>

#include <QColor>
#include <QVector>
#include <QtGlobal>

#include <optional>

namespace QindaQt::AppAppearance {

enum class ColorSchemePreference { System, Light, Dark };

struct AppearancePreference final {
  QString themeId;
  ColorSchemePreference colorScheme = ColorSchemePreference::System;
};

[[nodiscard]] std::optional<ColorSchemePreference>
colorSchemeFromToken(const QString &token);
[[nodiscard]] QString colorSchemeToken(ColorSchemePreference scheme);

// Pure shared policy for shell, Settings previews, and first-party apps.
// A compatible installed preference wins; otherwise its authored light/dark
// twin (ADR-0284: `variants` in the theme, used only when installed and of
// the wanted scheme), then the matching built-in, then another compatible
// installed theme. Following the system keeps the selection unless a known
// platform scheme calls for its twin. High contrast is compatible with
// either scheme so selecting it is not undone by ambient platform changes.
[[nodiscard]] std::optional<Themes::ThemeSpec>
resolveAppearanceTheme(const QVector<Themes::ThemeSpec> &installed,
                       const AppearancePreference &preference,
                       Qt::ColorScheme platformScheme);

} // namespace QindaQt::AppAppearance
