// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_preview.h"
#include "qindaqt/app_appearance/appearance_resolver.h"

#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_spec.h"

namespace QindaQt::Apps::SettingsAppearance {
AppearancePreview::AppearancePreview(QVector<Themes::ThemeSpec> installedThemes)
    : m_themes(std::move(installedThemes)) {
    m_previewMaps.reserve(static_cast<size_t>(m_themes.size()));
    for (const auto &theme : m_themes) {
    const auto derived = DesignTokens::DesignTokenDeriver::derive(theme, {});
        m_previewMaps.push_back(derived.ok() ? derived.tokens->toVariantMap()
                                             : QVariantMap{});
    }
}

AppearanceResolution
AppearancePreview::resolve(const AppearanceValues &values,
                           Qt::ColorScheme platformScheme) const {
    AppearanceResolution resolution;
  const auto scheme = values.colorScheme == ColorSchemePreference::Light
                          ? AppAppearance::ColorSchemePreference::Light
                      : values.colorScheme == ColorSchemePreference::Dark
                          ? AppAppearance::ColorSchemePreference::Dark
                          : AppAppearance::ColorSchemePreference::System;
  const auto resolved = AppAppearance::resolveAppearanceTheme(
      m_themes, {.themeId = values.themeId, .colorScheme = scheme},
      platformScheme);
  if (!resolved)
            return resolution;
  const QString fallback = resolved->id;
    for (int index = 0; index < m_themes.size(); ++index) {
        if (m_themes.at(index).id == fallback) {
            resolution.themeIndex = index;
      resolution.configuredInstalled = fallback == values.themeId;
      if (!resolution.configuredInstalled)
            resolution.fallbackThemeId = fallback;
            return resolution;
        }
    }
    return resolution;
}

DesignTokens::AccessibilityInputs
AppearancePreview::accessibilityInputs(const AppearanceValues &values,
                                       const Themes::ThemeSpec &theme) const {
    // Accessibility-domain settings (text scale, reduced motion/transparency)
    // belong to their own Settings route and must not be guessed here. Font
    // size is an explicit caller input; high contrast follows the dedicated
    // theme variant, matching the text-editor composition.
    DesignTokens::AccessibilityInputs inputs;
    inputs.basePointSize = values.fontPointSize;
    inputs.highContrast = theme.variant == QStringLiteral("high-contrast");
    return inputs;
}

} // namespace QindaQt::Apps::SettingsAppearance
