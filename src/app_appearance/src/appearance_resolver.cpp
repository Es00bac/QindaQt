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

const Themes::ThemeSpec *findTheme(const QVector<Themes::ThemeSpec> &installed,
                                   const QString &id) {
  if (id.isEmpty())
    return nullptr;
  for (const auto &theme : installed)
    if (theme.id == id)
      return &theme;
  return nullptr;
}

// ADR-0284: the selected theme's authored twin for the wanted scheme, when it
// is installed and really of that scheme. Nothing otherwise, so a missing or
// mislabelled pair falls back exactly as an unpaired theme does.
const Themes::ThemeSpec *twinFor(const QVector<Themes::ThemeSpec> &installed,
                                 const Themes::ThemeSpec &selected, bool dark) {
  const auto *twin =
      findTheme(installed, dark ? selected.darkVariant : selected.lightVariant);
  return twin && compatible(*twin, dark) ? twin : nullptr;
}
} // namespace

std::optional<Themes::ThemeSpec>
resolveAppearanceTheme(const QVector<Themes::ThemeSpec> &installed,
                       const AppearancePreference &preference,
                       Qt::ColorScheme platformScheme) {
  const auto *selected = findTheme(installed, preference.themeId);
  if (preference.colorScheme == ColorSchemePreference::System && selected) {
    // Following the system keeps the selection as is, unless the theme
    // pairs itself with a twin for the platform's known scheme (ADR-0284).
    const bool platformDark = platformScheme == Qt::ColorScheme::Dark;
    if (platformScheme != Qt::ColorScheme::Unknown &&
        !compatible(*selected, platformDark))
      if (const auto *twin = twinFor(installed, *selected, platformDark))
        return *twin;
    return *selected;
  }
  const bool dark = wantsDark(preference.colorScheme, platformScheme);
  if (selected && compatible(*selected, dark))
    return *selected;
  if (selected)
    if (const auto *twin = twinFor(installed, *selected, dark))
      return *twin;
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
