// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"

#include "qindaqt/decoration_painter/decoration_painter.h"

#include "native_palette.h"

#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_spec.h"

#include <QPalette>

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

QVariantList AppearanceSettingsModel::bundledWallpapers() const
{
    return m_bundledWallpapers;
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
    if (m_resolution.fallbackThemeId.isEmpty() || m_resolution.themeIndex < 0) {
        return {};
    }
    return QStringLiteral(
               "Configured theme '%1' does not match the selected color scheme; previewing '%2'")
        .arg(m_draft.themeId, resolvedThemeId());
}

QVariantList AppearanceSettingsModel::previewQtPalette() const
{
    if (m_resolution.themeIndex < 0
        || m_resolution.themeIndex >= m_preview.themes().size()) {
        return {};
    }
    const auto &theme = m_preview.themes().at(m_resolution.themeIndex);
    const auto native = QtTheme::nativeAppearance(
        theme, m_preview.accessibilityInputs(m_draft, theme));
    if (!native.has_value()) {
        return {};
    }
    static const struct {
        QPalette::ColorRole role;
        const char *label;
    } kRoles[] = {
        {QPalette::Window, "Window"},
        {QPalette::WindowText, "Window text"},
        {QPalette::Base, "Input fields"},
        {QPalette::Text, "Text"},
        {QPalette::Button, "Buttons"},
        {QPalette::ButtonText, "Button text"},
        {QPalette::Highlight, "Selection"},
        {QPalette::HighlightedText, "Selected text"},
        {QPalette::ToolTipBase, "Tooltips"},
        {QPalette::Link, "Links"},
    };
    QVariantList entries;
    entries.reserve(std::size(kRoles));
    for (const auto &entry : kRoles) {
        entries.append(QVariantMap{
            {QStringLiteral("role"), QString::fromLatin1(entry.label)},
            {QStringLiteral("color"),
             native->palette.color(QPalette::Normal, entry.role).name()},
        });
    }
    return entries;
}

QVariantMap AppearanceSettingsModel::previewChrome() const
{
    if (m_resolution.themeIndex < 0
        || m_resolution.themeIndex >= m_preview.themes().size()) {
        return {};
    }
    // The same derivation the compositor publishes to every decoration.
    return Decoration::DecorationChrome::fromTheme(
               m_preview.themes().at(m_resolution.themeIndex))
        .toVariantMap();
}

QVariantMap AppearanceSettingsModel::previewToolkitPalette() const
{
    if (m_resolution.themeIndex < 0
        || m_resolution.themeIndex >= m_preview.themes().size()) {
        return {};
    }
    const auto &theme = m_preview.themes().at(m_resolution.themeIndex);
    const auto native = QtTheme::nativeAppearance(
        theme, m_preview.accessibilityInputs(m_draft, theme));
    if (!native.has_value()) {
        return {};
    }
    const auto &palette = native->palette;
    static const struct {
        const char *key;
        QPalette::ColorRole role;
    } kRoles[] = {
        {"window", QPalette::Window},         {"windowText", QPalette::WindowText},
        {"base", QPalette::Base},             {"text", QPalette::Text},
        {"button", QPalette::Button},         {"buttonText", QPalette::ButtonText},
        {"highlight", QPalette::Highlight},   {"highlightedText", QPalette::HighlightedText},
        {"mid", QPalette::Mid},               {"dark", QPalette::Dark},
        {"light", QPalette::Light},           {"placeholderText", QPalette::PlaceholderText},
    };
    QVariantMap map;
    for (const auto &entry : kRoles) {
        map.insert(QString::fromLatin1(entry.key), palette.color(QPalette::Normal, entry.role));
    }
    map.insert(QStringLiteral("disabledText"),
               palette.color(QPalette::Disabled, QPalette::Text));
    return map;
}

QFont AppearanceSettingsModel::previewToolkitFont() const
{
    if (m_resolution.themeIndex < 0
        || m_resolution.themeIndex >= m_preview.themes().size()) {
        return {};
    }
    const auto &theme = m_preview.themes().at(m_resolution.themeIndex);
    const auto native = QtTheme::nativeAppearance(
        theme, m_preview.accessibilityInputs(m_draft, theme));
    if (!native.has_value()) {
        return {};
    }
    QFont font = native->font;
    // The draft font family and size win so the preview follows the Fonts
    // tab before Apply, exactly as the platform theme will after it.
    if (!m_draft.fontFamily.isEmpty()) {
        font.setFamily(m_draft.fontFamily);
    }
    if (m_draft.fontPointSize > 0.0) {
        font.setPointSizeF(m_draft.fontPointSize);
    }
    return font;
}

QColor AppearanceSettingsModel::previewCanvasColor() const
{
    if (m_resolution.themeIndex < 0
        || m_resolution.themeIndex >= m_preview.themes().size()) {
        return {};
    }
    const auto &theme = m_preview.themes().at(m_resolution.themeIndex);
    const auto canvas = theme.colors.value(QStringLiteral("canvas"));
    return canvas.isValid() ? canvas : theme.colors.value(QStringLiteral("surface"));
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
