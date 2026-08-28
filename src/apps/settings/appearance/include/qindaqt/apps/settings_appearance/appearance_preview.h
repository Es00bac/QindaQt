// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/apps/settings_appearance/appearance_values.h"

#include "qindaqt/design_tokens/accessibility_inputs.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/themes/theme_spec.h"

#include <QColor>
#include <QtGlobal>

#include <memory>
#include <vector>

namespace QindaQt::Apps::SettingsAppearance {

// Complete per-theme token maps for QindaQt.Controls ThemeCard previews. The
// maps follow the published QST generation shape (DesignTokens::toVariantMap)
// so controls never see a partial preview hybrid.
using ThemePreviewTokenMaps = std::vector<QVariantMap>;

// Where a draft's configured theme resolves inside the installed catalog.
struct AppearanceResolution final {
    int themeIndex = -1;
    bool configuredInstalled = false;
    // Non-empty when the configured theme id is missing and a scheme-based
    // built-in fallback was chosen instead.
    QString fallbackThemeId;
};

// AGENT-CONTRACT: Pure projection between validated appearance values and
// QST-1. It owns no settings client, no persistence, and no publication; the
// settings model composes it. Resolution order: configured theme id when
// installed; otherwise the built-in dark/light id matching the preference
// (system follows the platform scheme); otherwise the first installed theme.
class AppearancePreview final {
public:
    // Installed themes must be loader-valid (catalog-provided) and non-empty;
    // construction derives every theme's tokens once up front.
    explicit AppearancePreview(QVector<Themes::ThemeSpec> installedThemes);

    [[nodiscard]] const QVector<Themes::ThemeSpec> &themes() const noexcept
    {
        return m_themes;
    }

    [[nodiscard]] const ThemePreviewTokenMaps &previewMaps() const noexcept
    {
        return m_previewMaps;
    }

    [[nodiscard]] AppearanceResolution
    resolve(const AppearanceValues &values,
            Qt::ColorScheme platformScheme) const;

    [[nodiscard]] DesignTokens::AccessibilityInputs
    accessibilityInputs(const AppearanceValues &values,
                        const Themes::ThemeSpec &theme) const;

private:
    QVector<Themes::ThemeSpec> m_themes;
    ThemePreviewTokenMaps m_previewMaps;
};

} // namespace QindaQt::Apps::SettingsAppearance
