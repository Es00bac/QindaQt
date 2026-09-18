// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"

#include "qindaqt/decoration_painter/decoration_painter.h"

#include "native_palette.h"

#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_spec.h"

#include <QFileInfo>
#include <QPalette>

namespace QindaQt::Apps::SettingsAppearance {

QVariantList AppearanceSettingsModel::installedThemes() const
{
    QVariantList entries;
    const auto &themes = m_preview.themes();
    const auto &maps = m_preview.previewMaps();
    entries.reserve(themes.size());
    for (int index = 0; index < themes.size(); ++index) {
        const auto &theme = themes.at(index);
        // Each card paints the theme's own pairing (ADR-0207) under the
        // shipped arrangement, so the grid shows themes as they install.
        const auto paired = Decoration::selectDecorationTheme(theme, m_decorations,
                                                              QStringLiteral("theme"));
        entries.append(QVariantMap{
            {QStringLiteral("id"), theme.id},
            {QStringLiteral("name"), theme.name},
            {QStringLiteral("variant"), theme.variant},
            {QStringLiteral("previewTokens"), maps.at(static_cast<size_t>(index))},
            {QStringLiteral("previewChrome"),
             Decoration::resolveWindowChrome(theme, paired, Decoration::ChromePreferences{})
                 .toVariantMap()},
            {QStringLiteral("previewContainerStyle"),
             Decoration::containerStyleToVariantMap(Decoration::resolveContainerStyle(
                 theme, paired, Decoration::ChromePreferences{}))},
        });
    }
    return entries;
}

QVariantList AppearanceSettingsModel::decorationDocuments() const
{
    if (m_resolution.themeIndex < 0
        || m_resolution.themeIndex >= m_preview.themes().size()) {
        return {};
    }
    const auto &theme = m_preview.themes().at(m_resolution.themeIndex);
    const auto entry = [&theme, this](const QString &id, const QString &name,
                                      const QString &description,
                                      const std::optional<Themes::DecorationThemeSpec> &document) {
        return QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("description"), description},
            {QStringLiteral("previewChrome"),
             Decoration::resolveWindowChrome(theme, document, m_draft.chrome).toVariantMap()},
            {QStringLiteral("previewContainerStyle"),
             Decoration::containerStyleToVariantMap(
                 Decoration::resolveContainerStyle(theme, document, m_draft.chrome))},
        };
    };
    QVariantList entries;
    const auto paired = Decoration::selectDecorationTheme(theme, m_decorations,
                                                          QStringLiteral("theme"));
    entries.append(entry(QStringLiteral("theme"), tr("Theme"),
                         paired ? tr("Paired with %1").arg(paired->name)
                                : tr("The theme's own chrome"),
                         paired));
    for (const auto &document : m_decorations) {
        entries.append(entry(document.id, document.name, document.description, document));
    }
    return entries;
}

QUrl AppearanceSettingsModel::previewWallpaper() const
{
    const QString value = m_draft.wallpaper;
    if (value.isEmpty()) {
        return {};
    }
    for (const auto &entry : m_bundledWallpapers) {
        const auto map = entry.toMap();
        if (map.value(QStringLiteral("value")).toString() == value) {
            return map.value(QStringLiteral("previewUrl")).toUrl();
        }
    }
    const QFileInfo file(value);
    return file.isAbsolute() && file.isFile() ? QUrl::fromLocalFile(file.absoluteFilePath())
                                              : QUrl();
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
    // The same resolution the compositor publishes to every decoration:
    // the draft's decoration document (ADR-0207) and window arrangement
    // (ADR-0129) over the resolved theme.
    const auto &theme = m_preview.themes().at(m_resolution.themeIndex);
    return Decoration::resolveWindowChrome(
               theme,
               Decoration::selectDecorationTheme(theme, m_decorations,
                                                 m_draft.chrome.windowDecoration),
               m_draft.chrome)
        .toVariantMap();
}

QVariantMap AppearanceSettingsModel::previewContainerStyle() const
{
    if (m_resolution.themeIndex < 0
        || m_resolution.themeIndex >= m_preview.themes().size()) {
        return {};
    }
    const auto &theme = m_preview.themes().at(m_resolution.themeIndex);
    return Decoration::containerStyleToVariantMap(Decoration::resolveContainerStyle(
        theme,
        Decoration::selectDecorationTheme(theme, m_decorations,
                                          m_draft.chrome.containerDecoration),
        m_draft.chrome));
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
