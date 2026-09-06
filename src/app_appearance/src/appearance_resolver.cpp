// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/app_appearance/appearance_resolver.h>

namespace QindaQt::AppAppearance {

std::optional<ColorSchemePreference>
colorSchemeFromToken(const QString &token) {
  if (token == QLatin1String("system"))
    return ColorSchemePreference::System;
  if (token == QLatin1String("light"))
    return ColorSchemePreference::Light;
  if (token == QLatin1String("dark"))
    return ColorSchemePreference::Dark;
  return std::nullopt;
}

QString colorSchemeToken(ColorSchemePreference scheme) {
  switch (scheme) {
  case ColorSchemePreference::System:
    return QStringLiteral("system");
  case ColorSchemePreference::Light:
    return QStringLiteral("light");
  case ColorSchemePreference::Dark:
    return QStringLiteral("dark");
  }
  return QStringLiteral("system");
}

namespace {
bool wantsDark(ColorSchemePreference preference, Qt::ColorScheme platform) {
  return preference == ColorSchemePreference::Dark ||
         (preference == ColorSchemePreference::System &&
          platform != Qt::ColorScheme::Light);
}

bool compatible(const Themes::ThemeSpec &theme, bool dark) {
  if (theme.variant == QLatin1String("high-contrast"))
    return true;
  const bool themeDark = theme.variant == QLatin1String("dark") ||
                         theme.variant == QLatin1String("dusk");
  return themeDark == dark;
}
} // namespace

std::optional<Themes::ThemeSpec>
resolveAppearanceTheme(const QVector<Themes::ThemeSpec> &installed,
                       const AppearancePreference &preference,
                       Qt::ColorScheme platformScheme) {
  const bool dark = wantsDark(preference.colorScheme, platformScheme);
  for (const auto &theme : installed) {
    if (theme.id == preference.themeId && compatible(theme, dark))
      return theme;
  }
  const QString builtIn =
      dark ? QStringLiteral("qinda-dark") : QStringLiteral("qinda-light");
  for (const auto &theme : installed)
    if (theme.id == builtIn)
      return theme;
  for (const auto &theme : installed)
    if (compatible(theme, dark))
      return theme;
  return std::nullopt;
}

} // namespace QindaQt::AppAppearance
