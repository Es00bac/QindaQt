// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_preview.h"

#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_spec.h"

namespace QindaQt::Apps::SettingsAppearance {
namespace {

// AGENT-CONTRACT: These built-in ids are the deterministic scheme fallbacks
// for the dark/light/system preference. They exist in data/themes; if a
// deployment renames them the last-resort branch (first installed theme)
// keeps resolution total, but the fallback contract must be re-documented.
const QLatin1String DarkThemeId("qinda-dark");
const QLatin1String LightThemeId("qinda-light");

[[nodiscard]] QString schemeThemeId(const AppearanceValues &values,
                                    Qt::ColorScheme platformScheme)
{
    switch (values.colorScheme) {
    case ColorSchemePreference::Dark:
        return QLatin1String(DarkThemeId);
    case ColorSchemePreference::Light:
        return QLatin1String(LightThemeId);
    case ColorSchemePreference::System:
        return platformScheme == Qt::ColorScheme::Dark
            ? QString(QLatin1String(DarkThemeId))
            : QString(QLatin1String(LightThemeId));
    }
    return QLatin1String(DarkThemeId);
}

} // namespace

AppearancePreview::AppearancePreview(QVector<Themes::ThemeSpec> installedThemes)
    : m_themes(std::move(installedThemes))
{
    m_previewMaps.reserve(static_cast<size_t>(m_themes.size()));
    for (const auto &theme : m_themes) {
        const auto derived =
            DesignTokens::DesignTokenDeriver::derive(theme, {});
        m_previewMaps.push_back(derived.ok() ? derived.tokens->toVariantMap()
                                             : QVariantMap{});
    }
}

AppearanceResolution AppearancePreview::resolve(
    const AppearanceValues &values, Qt::ColorScheme platformScheme) const
{
    AppearanceResolution resolution;
    for (int index = 0; index < m_themes.size(); ++index) {
        if (m_themes.at(index).id == values.themeId) {
            resolution.themeIndex = index;
            resolution.configuredInstalled = true;
            return resolution;
        }
    }

    const QString fallback = schemeThemeId(values, platformScheme);
    for (int index = 0; index < m_themes.size(); ++index) {
        if (m_themes.at(index).id == fallback) {
            resolution.themeIndex = index;
            resolution.fallbackThemeId = fallback;
            return resolution;
        }
    }
    if (!m_themes.isEmpty()) {
        resolution.themeIndex = 0;
        resolution.fallbackThemeId = m_themes.first().id;
    }
    return resolution;
}

DesignTokens::AccessibilityInputs AppearancePreview::accessibilityInputs(
    const AppearanceValues &values, const Themes::ThemeSpec &theme) const
{
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
