// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"

#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_spec.h"

namespace QindaQt::Apps::SettingsAppearance {

QVariantList AppearanceSettingsModel::installedThemes() const
{
    QVariantList entries;
    const auto &themes = m_preview.themes();
    const auto &maps = m_preview.previewMaps();
    entries.reserve(themes.size());
    for (int index = 0; index < themes.size(); ++index) {
        entries.append(QVariantMap{
            {QStringLiteral("id"), themes.at(index).id},
            {QStringLiteral("name"), themes.at(index).name},
            {QStringLiteral("variant"), themes.at(index).variant},
            {QStringLiteral("previewTokens"), maps.at(static_cast<size_t>(index))},
        });
    }
    return entries;
}

QString AppearanceSettingsModel::resolvedThemeId() const
{
    if (m_resolution.themeIndex < 0
        || m_resolution.themeIndex >= m_preview.themes().size()) {
        return {};
    }
    return m_preview.themes().at(m_resolution.themeIndex).id;
}

bool AppearanceSettingsModel::configuredThemeInstalled() const
{
    return m_resolution.configuredInstalled;
}

QString AppearanceSettingsModel::fallbackNotice() const
{
    if (m_resolution.configuredInstalled || m_resolution.themeIndex < 0) {
        return {};
    }
    return QStringLiteral(
               "Configured theme '%1' is not installed; previewing '%2'")
        .arg(m_draft.themeId, resolvedThemeId());
}

QSet<QString> AppearanceSettingsModel::installedThemeIds() const
{
    QSet<QString> ids;
    const auto &themes = m_preview.themes();
    ids.reserve(themes.size());
    for (const auto &theme : themes) {
        ids.insert(theme.id);
    }
    return ids;
}

void AppearanceSettingsModel::refreshValidationAndPreview()
{
    m_validation = validateAppearanceDraft(m_draft, installedThemeIds());
    m_resolution = m_preview.resolve(m_draft, m_platformScheme);
    publishPreviewTokens();
    // AGENT-NOTE: applyAvailable is a composite of state, draft dirt, and
    // draft validity but a Q_PROPERTY allows one NOTIFY signal; stateChanged
    // doubles as its change notification here.
    Q_EMIT stateChanged();
    Q_EMIT draftChanged();
    Q_EMIT previewChanged();
}

void AppearanceSettingsModel::publishPreviewTokens()
{
    if (m_previewFacade == nullptr || m_resolution.themeIndex < 0) {
        return;
    }
    const auto &theme = m_preview.themes().at(m_resolution.themeIndex);
    // AGENT-GUARD: Publication must always carry one complete immutable
    // generation derived from the draft; publishing derived roles piecemeal
    // would let controls render a hybrid of two themes.
    const auto derived = DesignTokens::DesignTokenDeriver::derive(
        theme, m_preview.accessibilityInputs(m_draft, theme));
    if (!derived.ok()) {
        return;
    }
    QString error;
    if (!m_previewFacade->publish(derived.tokens, &error)) {
        // A refused publication leaves the last confirmed generation intact
        // and is a preview-only loss; state truth never depends on it.
        qWarning("appearance preview publication failed: %s", qPrintable(error));
    }
}

} // namespace QindaQt::Apps::SettingsAppearance
