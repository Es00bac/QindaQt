// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include "qindaqt/themes/decoration_theme_loader.h"

#include <utility>

// User arrangement for the two chrome sets (ADR-0129): the preference model,
// its Settings1 tokens, and the resolution of a theme plus preferences into
// the published window chrome and the container chrome style.
namespace QindaQt::Decoration {
namespace {

using HybridChrome::ButtonSide;
using HybridChrome::ButtonStyle;
using HybridChrome::ChromeStyle;
using HybridChrome::TabVisualDirection;

struct PreferenceField final {
    QLatin1String key;
    QString ChromePreferences::*member;
    QStringList tokens; // default first
};

const QList<PreferenceField> &preferenceFields()
{
    const auto theme = QStringLiteral("theme");
    static const QList<PreferenceField> fields{
        {ChromePreferenceKeys::WindowButtonStyle, &ChromePreferences::windowButtonStyle,
         {theme, QStringLiteral("traffic-lights"), QStringLiteral("flat"),
          QStringLiteral("glyph")}},
        {ChromePreferenceKeys::WindowButtonSide, &ChromePreferences::windowButtonSide,
         {theme, QStringLiteral("left"), QStringLiteral("right")}},
        {ChromePreferenceKeys::WindowButtons, &ChromePreferences::windowButtons,
         {QStringLiteral("all"), QStringLiteral("minimize-close"), QStringLiteral("close")}},
        {ChromePreferenceKeys::WindowTitleAlignment, &ChromePreferences::windowTitleAlignment,
         {QStringLiteral("center"), QStringLiteral("left")}},
        {ChromePreferenceKeys::ContainerButtonStyle, &ChromePreferences::containerButtonStyle,
         {theme, QStringLiteral("traffic-lights"), QStringLiteral("flat")}},
        {ChromePreferenceKeys::ContainerButtonSide, &ChromePreferences::containerButtonSide,
         {theme, QStringLiteral("left"), QStringLiteral("right")}},
        {ChromePreferenceKeys::ContainerTabOrder, &ChromePreferences::containerTabOrder,
         {theme, QStringLiteral("left-to-right"), QStringLiteral("right-to-left")}},
        {ChromePreferenceKeys::ContainerButtonGlyphs, &ChromePreferences::containerButtonGlyphs,
         {theme, QStringLiteral("always"), QStringLiteral("hover")}},
    };
    return fields;
}

const PreferenceField *findField(const QString &key)
{
    for (const auto &field : preferenceFields()) {
        if (key == field.key) {
            return &field;
        }
    }
    return nullptr;
}

QColor styleColor(const QVariantMap &map, const char *name, const QColor &fallback)
{
    const auto value = map.value(QString::fromLatin1(name));
    if (value.canConvert<QColor>()) {
        const auto color = value.value<QColor>();
        if (color.isValid()) {
            return color;
        }
    }
    return fallback;
}

} // namespace

bool isDecorationChoice(const QString &value)
{
    // "theme" or a decoration document id (the icon-theme grammar).
    if (value.isEmpty() || value.size() > 128 || value.contains(QStringLiteral(".."))) {
        return false;
    }
    for (const QChar character : value) {
        const ushort code = character.unicode();
        const bool accepted = (code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z')
            || (code >= '0' && code <= '9') || code == '.' || code == '_' || code == '-';
        if (!accepted) {
            return false;
        }
    }
    return true;
}

QStringList ChromePreferences::settingsKeys()
{
    QStringList keys;
    for (const auto &field : preferenceFields()) {
        keys.append(QString(field.key));
    }
    return keys;
}

QStringList ChromePreferences::decorationKeys()
{
    return {QString(ChromePreferenceKeys::WindowDecoration),
            QString(ChromePreferenceKeys::ContainerDecoration)};
}

QStringList ChromePreferences::tokens(const QString &key)
{
    const auto *field = findField(key);
    return field != nullptr ? field->tokens : QStringList{};
}

ChromePreferences ChromePreferences::fromSettingsValues(const QVariantMap &values)
{
    ChromePreferences preferences;
    for (const auto &field : preferenceFields()) {
        const auto value = values.value(QString(field.key));
        if (value.metaType().id() == QMetaType::QString
            && field.tokens.contains(value.toString())) {
            preferences.*field.member = value.toString();
        }
    }
    for (const auto &[key, member] :
         {std::pair{ChromePreferenceKeys::WindowDecoration, &ChromePreferences::windowDecoration},
          std::pair{ChromePreferenceKeys::ContainerDecoration,
                    &ChromePreferences::containerDecoration}}) {
        const auto value = values.value(QString(key));
        if (value.metaType().id() == QMetaType::QString && isDecorationChoice(value.toString())) {
            preferences.*member = value.toString();
        }
    }
    return preferences;
}

QVariantMap ChromePreferences::toSettingsValues() const
{
    QVariantMap values;
    for (const auto &field : preferenceFields()) {
        values.insert(QString(field.key), this->*field.member);
    }
    values.insert(QString(ChromePreferenceKeys::WindowDecoration), windowDecoration);
    values.insert(QString(ChromePreferenceKeys::ContainerDecoration), containerDecoration);
    return values;
}

QString ChromePreferences::token(const QString &key) const
{
    if (key == ChromePreferenceKeys::WindowDecoration) {
        return windowDecoration;
    }
    if (key == ChromePreferenceKeys::ContainerDecoration) {
        return containerDecoration;
    }
    const auto *field = findField(key);
    return field != nullptr ? this->*field->member : QString{};
}

bool ChromePreferences::setToken(const QString &key, const QString &value)
{
    if (key == ChromePreferenceKeys::WindowDecoration
        || key == ChromePreferenceKeys::ContainerDecoration) {
        if (!isDecorationChoice(value)) {
            return false;
        }
        (key == ChromePreferenceKeys::WindowDecoration ? windowDecoration
                                                       : containerDecoration) = value;
        return true;
    }
    const auto *field = findField(key);
    if (field == nullptr || !field->tokens.contains(value)) {
        return false;
    }
    this->*field->member = value;
    return true;
}

DecorationChrome applyWindowPreferences(DecorationChrome chrome,
                                        const ChromePreferences &preferences)
{
    if (preferences.windowButtonStyle != QLatin1String("theme")) {
        chrome.buttonStyle = preferences.windowButtonStyle;
    }
    if (preferences.windowButtonSide != QLatin1String("theme")) {
        chrome.buttonSide = preferences.windowButtonSide;
    }
    chrome.buttons = preferences.windowButtons;
    chrome.titleAlignment = preferences.windowTitleAlignment;
    return chrome;
}

DecorationChrome resolveWindowChrome(const Themes::ThemeSpec &theme,
                                     const ChromePreferences &preferences)
{
    return applyWindowPreferences(DecorationChrome::fromTheme(theme), preferences);
}

namespace {

ChromeStyle applyContainerPreferences(ChromeStyle style, bool themeHoverGlyphs,
                                      const ChromePreferences &preferences);

// The color theme's own container arrangement: an unauthored theme keeps the
// Qinda macOS container arrangement every theme shipped with, so defaults
// stay byte-identical. `themeHoverGlyphs` reports the theme's hover choice
// for the "theme" preference token.
ChromeStyle containerStyleForTheme(const Themes::ThemeSpec &theme, bool *themeHoverGlyphs)
{
    ChromeStyle style = ChromeStyle::qindaMacOS(chromePaletteForTheme(theme));
    *themeHoverGlyphs = style.hoverGlyphs;
    if (theme.decoration.authored) {
        const auto &decoration = theme.decoration;
        style.buttonSide = decoration.buttonPlacement == QLatin1String("left")
            ? ButtonSide::Left : ButtonSide::Right;
        style.tabDirection = decoration.tabDirection == QLatin1String("right-to-left")
            ? TabVisualDirection::RightToLeft : TabVisualDirection::LeftToRight;
        style.buttonStyle = decoration.buttonStyle == QLatin1String("traffic-lights")
            ? ButtonStyle::TrafficLights : ButtonStyle::Symbols;
        *themeHoverGlyphs = decoration.hoverGlyphs;
    }
    style.material = containerMaterialForTheme(theme);
    return style;
}

} // namespace

HybridChrome::ChromeMaterial containerMaterialForTheme(const Themes::ThemeSpec &theme)
{
    HybridChrome::ChromeMaterial material;
    if (theme.schemaVersion < 2) {
        return material;
    }
    const auto surface = theme.surface(QString(Themes::SurfaceNames::ContainerChrome));
    material.opacity = surface.opacity;
    material.tint = surface.tint;
    material.border = surface.border;
    material.highlight = surface.highlight;
    return material;
}

DecorationChrome applyDecorationTheme(DecorationChrome chrome,
                                      const Themes::DecorationThemeSpec &document)
{
    const auto &decoration = document.decoration;
    chrome.buttonStyle = decoration.buttonStyle;
    chrome.buttonSide = decoration.buttonPlacement;
    // Colors apply only where the document authors them; the palette's own
    // button and title colors otherwise stay, so a neutral document serves
    // every color theme.
    const auto authoredColor = [](QColor *target, const QColor &authored) {
        if (authored.isValid()) {
            *target = authored;
        }
    };
    authoredColor(&chrome.close, decoration.closeColor);
    authoredColor(&chrome.minimize, decoration.minimizeColor);
    authoredColor(&chrome.maximize, decoration.maximizeColor);
    authoredColor(&chrome.restore, decoration.restoreColor);
    if (decoration.titleBarColor.isValid()) {
        chrome.titleBar = decoration.titleBarColor;
        chrome.titleBarInactive = decoration.titleBarInactiveColor.isValid()
            ? decoration.titleBarInactiveColor : decoration.titleBarColor;
    } else if (chrome.wornLuna() && document.titleMaterial.authored) {
        // A material document over a Luna theme paints the material, not the
        // theme's worn bar: the document owns the title surface.
        chrome.titleBar = QColor();
        chrome.titleBarInactive = QColor();
    }
    chrome.titleOpacity = document.titleMaterial.opacity;
    chrome.titleBlur = document.titleMaterial.blur;
    chrome.titleHighlight = document.titleMaterial.highlight;
    chrome.titleTint = document.titleMaterial.tint;
    chrome.cornerRadius = document.cornerRadius;
    chrome.shadowExtent = document.shadowExtent;
    chrome.shadowOpacity = document.shadowOpacity * document.titleMaterial.shadow;
    chrome.handleStyle = document.memberHandleStyle;
    return chrome;
}

ChromeStyle applyDecorationTheme(ChromeStyle style, const Themes::DecorationThemeSpec &document)
{
    const auto &decoration = document.decoration;
    style.buttonSide = decoration.buttonPlacement == QLatin1String("left")
        ? ButtonSide::Left : ButtonSide::Right;
    style.tabDirection = decoration.tabDirection == QLatin1String("right-to-left")
        ? TabVisualDirection::RightToLeft : TabVisualDirection::LeftToRight;
    style.buttonStyle = decoration.buttonStyle == QLatin1String("traffic-lights")
        ? ButtonStyle::TrafficLights : ButtonStyle::Symbols;
    style.hoverGlyphs = decoration.hoverGlyphs && style.buttonStyle == ButtonStyle::TrafficLights;
    if (decoration.closeColor.isValid()) {
        style.palette.close = decoration.closeColor;
    }
    if (decoration.minimizeColor.isValid()) {
        style.palette.minimize = decoration.minimizeColor;
    }
    if (decoration.maximizeColor.isValid()) {
        style.palette.maximize = decoration.maximizeColor;
    }
    style.material.opacity = document.titleMaterial.opacity;
    style.material.tint = document.titleMaterial.tint;
    style.material.border = document.titleMaterial.border;
    style.material.highlight = document.titleMaterial.highlight;
    style.material.squareBadge = document.containerBadgeStyle == QLatin1String("square");
    return style;
}

std::optional<Themes::DecorationThemeSpec>
selectDecorationTheme(const Themes::ThemeSpec &theme,
                      const QVector<Themes::DecorationThemeSpec> &installed,
                      const QString &preference)
{
    if (!preference.isEmpty() && preference != QLatin1String("theme")) {
        if (auto chosen = Themes::DecorationThemeLoader::find(installed, preference)) {
            return chosen;
        }
    }
    if (!theme.decorationTheme.isEmpty()) {
        return Themes::DecorationThemeLoader::find(installed, theme.decorationTheme);
    }
    return std::nullopt;
}

DecorationChrome resolveWindowChrome(const Themes::ThemeSpec &theme,
                                     const std::optional<Themes::DecorationThemeSpec> &document,
                                     const ChromePreferences &preferences)
{
    return decorateWindowChrome(DecorationChrome::fromTheme(theme), theme, document, preferences);
}

DecorationChrome decorateWindowChrome(DecorationChrome chrome, const Themes::ThemeSpec &theme,
                                      const std::optional<Themes::DecorationThemeSpec> &document,
                                      const ChromePreferences &preferences)
{
    if (theme.schemaVersion >= 2) {
        // A v2 color theme's own decoration surface: applied before any
        // document so a "theme" pairing still paints the authored material.
        const auto surface = theme.surface(QString(Themes::SurfaceNames::Decoration));
        chrome.titleOpacity = surface.opacity;
        chrome.titleBlur = surface.blur;
        chrome.titleHighlight = surface.highlight;
        chrome.titleTint = surface.tint;
        chrome.cornerRadius = theme.surfaceRadius(QString(Themes::SurfaceNames::Decoration));
        chrome.shadowOpacity = 0.30 * surface.shadow;
    }
    if (document) {
        chrome = applyDecorationTheme(chrome, *document);
    }
    return applyWindowPreferences(chrome, preferences);
}

ChromeStyle resolveContainerStyle(const Themes::ThemeSpec &theme,
                                  const std::optional<Themes::DecorationThemeSpec> &document,
                                  const ChromePreferences &preferences)
{
    bool themeHoverGlyphs = false;
    ChromeStyle style = containerStyleForTheme(theme, &themeHoverGlyphs);
    if (document) {
        style = applyDecorationTheme(style, *document);
        themeHoverGlyphs = document->decoration.hoverGlyphs;
    }
    return applyContainerPreferences(style, themeHoverGlyphs, preferences);
}

ChromeStyle resolveContainerStyle(const Themes::ThemeSpec &theme,
                                  const ChromePreferences &preferences)
{
    bool themeHoverGlyphs = false;
    const ChromeStyle style = containerStyleForTheme(theme, &themeHoverGlyphs);
    return applyContainerPreferences(style, themeHoverGlyphs, preferences);
}

namespace {

ChromeStyle applyContainerPreferences(ChromeStyle style, bool themeHoverGlyphs,
                                      const ChromePreferences &preferences)
{
    if (preferences.containerButtonStyle == QLatin1String("traffic-lights")) {
        style.buttonStyle = ButtonStyle::TrafficLights;
    } else if (preferences.containerButtonStyle == QLatin1String("flat")) {
        style.buttonStyle = ButtonStyle::Symbols;
    }
    if (preferences.containerButtonSide == QLatin1String("left")) {
        style.buttonSide = ButtonSide::Left;
    } else if (preferences.containerButtonSide == QLatin1String("right")) {
        style.buttonSide = ButtonSide::Right;
    }
    if (preferences.containerTabOrder == QLatin1String("left-to-right")) {
        style.tabDirection = TabVisualDirection::LeftToRight;
    } else if (preferences.containerTabOrder == QLatin1String("right-to-left")) {
        style.tabDirection = TabVisualDirection::RightToLeft;
    }
    if (preferences.containerButtonGlyphs == QLatin1String("always")) {
        style.hoverGlyphs = false;
    } else if (preferences.containerButtonGlyphs == QLatin1String("hover")) {
        style.hoverGlyphs = true;
    } else {
        // Hover-only glyphs belong to traffic lights; flat symbols without a
        // glyph would be blank plates, so "theme" keeps them visible.
        style.hoverGlyphs = style.buttonStyle == ButtonStyle::TrafficLights && themeHoverGlyphs;
    }
    return style;
}

} // namespace

QVariantMap containerStyleToVariantMap(const ChromeStyle &style)
{
    const auto &palette = style.palette;
    QVariantMap values = {
        {QStringLiteral("buttonSide"),
         style.buttonSide == ButtonSide::Left ? QStringLiteral("left") : QStringLiteral("right")},
        {QStringLiteral("tabDirection"),
         style.tabDirection == TabVisualDirection::RightToLeft
             ? QStringLiteral("right-to-left") : QStringLiteral("left-to-right")},
        {QStringLiteral("buttonStyle"),
         style.buttonStyle == ButtonStyle::TrafficLights
             ? QStringLiteral("traffic-lights") : QStringLiteral("symbols")},
        {QStringLiteral("hoverGlyphs"), style.hoverGlyphs},
        {QStringLiteral("surface"), palette.surface},
        {QStringLiteral("surfaceRaised"), palette.surfaceRaised},
        {QStringLiteral("border"), palette.border},
        {QStringLiteral("text"), palette.text},
        {QStringLiteral("textMuted"), palette.textMuted},
        {QStringLiteral("accent"), palette.accent},
        {QStringLiteral("close"), palette.close},
        {QStringLiteral("minimize"), palette.minimize},
        {QStringLiteral("maximize"), palette.maximize},
    };
    // Material keys (ADR-0207), omitted at their defaults.
    if (!qFuzzyCompare(style.material.opacity, 1.0)) {
        values.insert(QStringLiteral("materialOpacity"), style.material.opacity);
    }
    if (style.material.tint.isValid()) {
        values.insert(QStringLiteral("materialTint"), style.material.tint);
    }
    if (!qFuzzyCompare(style.material.border, 1.0)) {
        values.insert(QStringLiteral("materialBorder"), style.material.border);
    }
    if (style.material.highlight) {
        values.insert(QStringLiteral("materialHighlight"), true);
    }
    if (style.material.squareBadge) {
        values.insert(QStringLiteral("squareBadge"), true);
    }
    return values;
}

ChromeStyle containerStyleFromVariantMap(const QVariantMap &map)
{
    ChromeStyle style;
    const auto text = [&map](const char *name) {
        return map.value(QString::fromLatin1(name)).toString();
    };
    style.buttonSide = text("buttonSide") == QLatin1String("left") ? ButtonSide::Left
                                                                   : ButtonSide::Right;
    style.tabDirection = text("tabDirection") == QLatin1String("right-to-left")
        ? TabVisualDirection::RightToLeft : TabVisualDirection::LeftToRight;
    style.buttonStyle = text("buttonStyle") == QLatin1String("traffic-lights")
        ? ButtonStyle::TrafficLights : ButtonStyle::Symbols;
    style.hoverGlyphs = map.value(QStringLiteral("hoverGlyphs")).toBool();
    auto &palette = style.palette;
    palette.surface = styleColor(map, "surface", palette.surface);
    palette.surfaceRaised = styleColor(map, "surfaceRaised", palette.surfaceRaised);
    palette.border = styleColor(map, "border", palette.border);
    palette.text = styleColor(map, "text", palette.text);
    palette.textMuted = styleColor(map, "textMuted", palette.textMuted);
    palette.accent = styleColor(map, "accent", palette.accent);
    palette.close = styleColor(map, "close", palette.close);
    palette.minimize = styleColor(map, "minimize", palette.minimize);
    palette.maximize = styleColor(map, "maximize", palette.maximize);
    const auto bounded = [&map](const char *name, double fallback) {
        const auto value = map.value(QString::fromLatin1(name));
        if (value.metaType().id() != QMetaType::Double && value.metaType().id() != QMetaType::Int
            && value.metaType().id() != QMetaType::Float) {
            return fallback;
        }
        const double decoded = value.toDouble();
        return decoded >= 0.0 && decoded <= 1.0 ? decoded : fallback;
    };
    style.material.opacity = bounded("materialOpacity", 1.0);
    style.material.tint = styleColor(map, "materialTint", QColor());
    style.material.border = bounded("materialBorder", 1.0);
    style.material.highlight = map.value(QStringLiteral("materialHighlight")).toBool();
    style.material.squareBadge = map.value(QStringLiteral("squareBadge")).toBool();
    return style;
}

} // namespace QindaQt::Decoration
