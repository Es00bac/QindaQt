// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include "qindaqt/decoration_painter/decoration_button_style.h"

#include <QtMath>

// Container chrome resolution (ADR-0129, ADR-0207, ADR-0264): the color
// theme's own container arrangement, an optional decoration document, then
// the user's container preferences, and the variant-map codec the Settings
// preview reads. Split from chrome_preferences.cpp, which resolves windows.
namespace QindaQt::Decoration {
namespace {

using HybridChrome::ButtonSide;
using HybridChrome::ButtonStyle;
using HybridChrome::ChromeStyle;
using HybridChrome::TabVisualDirection;
using HybridChrome::TitleDoubleClickAction;

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

// ADR-0264: container buttons painted by a named style through the one
// style painter. Named styles read their row's order like windows (the
// right edge minimize, maximize, close), which is what Symbols lays out.
void applyNamedButtonStyle(ChromeStyle *style, const QString &name)
{
    const auto &named = decorationButtonStyle(name);
    const HybridChrome::ChromeMetrics metrics;
    style->buttonStyle = ButtonStyle::Symbols;
    style->namedButtonStyle = named.name;
    style->buttonPainter = &paintContainerButton;
    const qreal height = named.size > 0.0 ? named.size : metrics.titleBarHeight;
    style->buttonSize = QSizeF(height * named.aspect, height);
    style->buttonSpacing = named.shape == DecorationButtonShape::Pill ? 0.0 : named.spacing;
}

void clearNamedButtonStyle(ChromeStyle *style)
{
    style->namedButtonStyle.clear();
    style->buttonPainter = {};
    style->buttonSize = {};
    style->buttonSpacing = -1.0;
}

// A theme or document names its style. Pre-W19 names keep the built-in
// container plates they always had (Bliss authors "glyph" and its
// containers stay flat symbols, ADR-0129); a W19 name paints named buttons.
void applyAuthoredButtonStyle(ChromeStyle *style, const QString &name)
{
    const bool legacy = name == QLatin1String("symbols") || name == QLatin1String("glyph")
        || name == QLatin1String("flat") || name == QLatin1String("traffic-lights");
    if (legacy || !isDecorationButtonStyle(name)) {
        clearNamedButtonStyle(style);
        style->buttonStyle = name == QLatin1String("traffic-lights") ? ButtonStyle::TrafficLights
                                                                     : ButtonStyle::Symbols;
        return;
    }
    applyNamedButtonStyle(style, name);
}

QString doubleClickToken(TitleDoubleClickAction action)
{
    switch (action) {
    case TitleDoubleClickAction::Maximize:
        return QStringLiteral("maximize");
    case TitleDoubleClickAction::RollUp:
        return QStringLiteral("roll-up");
    case TitleDoubleClickAction::Minimize:
        return QStringLiteral("minimize");
    case TitleDoubleClickAction::None:
        break;
    }
    return QStringLiteral("none");
}

TitleDoubleClickAction doubleClickAction(const QString &token)
{
    if (token == QLatin1String("maximize")) {
        return TitleDoubleClickAction::Maximize;
    }
    if (token == QLatin1String("roll-up")) {
        return TitleDoubleClickAction::RollUp;
    }
    if (token == QLatin1String("minimize")) {
        return TitleDoubleClickAction::Minimize;
    }
    return TitleDoubleClickAction::None;
}

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
        applyAuthoredButtonStyle(&style, decoration.buttonStyle);
        *themeHoverGlyphs = decoration.hoverGlyphs;
    }
    style.material = containerMaterialForTheme(theme);
    return style;
}

ChromeStyle applyContainerPreferences(ChromeStyle style, bool themeHoverGlyphs,
                                      const ChromePreferences &preferences)
{
    const auto &buttonStyle = preferences.containerButtonStyle;
    if (buttonStyle == QLatin1String("traffic-lights")) {
        clearNamedButtonStyle(&style);
        style.buttonStyle = ButtonStyle::TrafficLights;
    } else if (buttonStyle == QLatin1String("flat")) {
        clearNamedButtonStyle(&style);
        style.buttonStyle = ButtonStyle::Symbols;
    } else if (buttonStyle != QLatin1String("theme")) {
        // Every other token, glyph included, is a named style (ADR-0264).
        applyNamedButtonStyle(&style, buttonStyle);
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
    } else if (style.buttonPainter) {
        // A named style reveals its glyphs the way it does on windows.
        style.hoverGlyphs = decorationButtonStyle(style.namedButtonStyle).glyphsOnHover;
    } else {
        // Hover-only glyphs belong to traffic lights; flat symbols without a
        // glyph would be blank plates, so "theme" keeps them visible.
        style.hoverGlyphs = style.buttonStyle == ButtonStyle::TrafficLights && themeHoverGlyphs;
    }
    // Title-bar options (ADR-0264): size and spacing scale the cells the
    // style resolved (the metrics' 14 px lights unless a named style set
    // its own); a joined capsule keeps no gaps.
    const bool sized = preferences.containerButtonSize != QLatin1String("theme");
    const bool spaced = preferences.containerButtonSpacing != QLatin1String("theme");
    if (sized || spaced) {
        const HybridChrome::ChromeMetrics metrics;
        const QSizeF cell = style.buttonSize.isEmpty()
            ? QSizeF(metrics.buttonExtent, metrics.buttonExtent) : style.buttonSize;
        const qreal scale = preferences.containerButtonSize == QLatin1String("small") ? 0.8
            : preferences.containerButtonSize == QLatin1String("large")               ? 1.25
                                                                                      : 1.0;
        style.buttonSize = QSizeF(qRound(cell.width() * scale), qRound(cell.height() * scale));
        const qreal gap = style.buttonSpacing >= 0.0 ? style.buttonSpacing : metrics.buttonSpacing;
        const qreal gapScale = preferences.containerButtonSpacing == QLatin1String("tight") ? 0.5
            : preferences.containerButtonSpacing == QLatin1String("roomy")                  ? 1.5
                                                                                            : 1.0;
        const bool joined = style.buttonPainter
            && decorationButtonStyle(style.namedButtonStyle).shape == DecorationButtonShape::Pill;
        style.buttonSpacing = joined ? 0.0 : static_cast<qreal>(qRound(gap * gapScale));
    }
    style.titleDoubleClick = doubleClickAction(preferences.containerTitleDoubleClick);
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

ChromeStyle applyDecorationTheme(ChromeStyle style, const Themes::DecorationThemeSpec &document)
{
    const auto &decoration = document.decoration;
    style.buttonSide = decoration.buttonPlacement == QLatin1String("left")
        ? ButtonSide::Left : ButtonSide::Right;
    style.tabDirection = decoration.tabDirection == QLatin1String("right-to-left")
        ? TabVisualDirection::RightToLeft : TabVisualDirection::LeftToRight;
    applyAuthoredButtonStyle(&style, decoration.buttonStyle);
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

QVariantMap containerStyleToVariantMap(const ChromeStyle &style)
{
    const auto &palette = style.palette;
    QVariantMap values = {
        {QStringLiteral("buttonSide"),
         style.buttonSide == ButtonSide::Left ? QStringLiteral("left") : QStringLiteral("right")},
        {QStringLiteral("tabDirection"),
         style.tabDirection == TabVisualDirection::RightToLeft
             ? QStringLiteral("right-to-left") : QStringLiteral("left-to-right")},
        // ADR-0264: a named style travels by name; the reader rebinds its
        // painter, which a variant map cannot carry.
        {QStringLiteral("buttonStyle"),
         style.buttonPainter && !style.namedButtonStyle.isEmpty() ? style.namedButtonStyle
         : style.buttonStyle == ButtonStyle::TrafficLights
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
    // Title-bar options (ADR-0264), omitted at their defaults.
    // Two plain numbers, not a QSizeF: the Settings preview's map passes
    // through QML, which keeps doubles exactly.
    if (!style.buttonSize.isEmpty()) {
        values.insert(QStringLiteral("buttonWidth"), style.buttonSize.width());
        values.insert(QStringLiteral("buttonHeight"), style.buttonSize.height());
    }
    if (style.buttonSpacing >= 0.0) {
        values.insert(QStringLiteral("buttonSpacing"), style.buttonSpacing);
    }
    if (style.titleDoubleClick != TitleDoubleClickAction::None) {
        values.insert(QStringLiteral("titleDoubleClick"), doubleClickToken(style.titleDoubleClick));
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
    const QString buttonStyle = text("buttonStyle");
    style.buttonStyle = buttonStyle == QLatin1String("traffic-lights")
        ? ButtonStyle::TrafficLights : ButtonStyle::Symbols;
    if (buttonStyle != QLatin1String("traffic-lights") && buttonStyle != QLatin1String("symbols")
        && isDecorationButtonStyle(buttonStyle)) {
        applyNamedButtonStyle(&style, buttonStyle);
    }
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
    const auto number = [&map](const char *name, double minimum, double maximum,
                               double fallback) {
        const auto value = map.value(QString::fromLatin1(name));
        if (value.metaType().id() != QMetaType::Double && value.metaType().id() != QMetaType::Int
            && value.metaType().id() != QMetaType::Float) {
            return fallback;
        }
        const double decoded = value.toDouble();
        return decoded >= minimum && decoded <= maximum ? decoded : fallback;
    };
    style.material.opacity = number("materialOpacity", 0.0, 1.0, 1.0);
    style.material.tint = styleColor(map, "materialTint", QColor());
    style.material.border = number("materialBorder", 0.0, 1.0, 1.0);
    style.material.highlight = map.value(QStringLiteral("materialHighlight")).toBool();
    style.material.squareBadge = map.value(QStringLiteral("squareBadge")).toBool();
    // Title-bar options (ADR-0264): tolerant, bounded, default when absent.
    const QSizeF cell(number("buttonWidth", 0.0, 96.0, 0.0),
                      number("buttonHeight", 0.0, 64.0, 0.0));
    if (!cell.isEmpty()) {
        style.buttonSize = cell;
    }
    if (map.contains(QStringLiteral("buttonSpacing"))) {
        style.buttonSpacing = number("buttonSpacing", 0.0, 64.0, style.buttonSpacing);
    }
    style.titleDoubleClick = doubleClickAction(text("titleDoubleClick"));
    return style;
}

} // namespace QindaQt::Decoration
