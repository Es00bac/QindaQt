// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

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

QStringList ChromePreferences::settingsKeys()
{
    QStringList keys;
    for (const auto &field : preferenceFields()) {
        keys.append(QString(field.key));
    }
    return keys;
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
    return preferences;
}

QVariantMap ChromePreferences::toSettingsValues() const
{
    QVariantMap values;
    for (const auto &field : preferenceFields()) {
        values.insert(QString(field.key), this->*field.member);
    }
    return values;
}

QString ChromePreferences::token(const QString &key) const
{
    const auto *field = findField(key);
    return field != nullptr ? this->*field->member : QString{};
}

bool ChromePreferences::setToken(const QString &key, const QString &value)
{
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

ChromeStyle resolveContainerStyle(const Themes::ThemeSpec &theme,
                                  const ChromePreferences &preferences)
{
    // AGENT-CONTRACT: an unauthored theme keeps the Qinda macOS container
    // arrangement every theme shipped with, so defaults stay byte-identical.
    ChromeStyle style = ChromeStyle::qindaMacOS(chromePaletteForTheme(theme));
    bool themeHoverGlyphs = style.hoverGlyphs;
    if (theme.decoration.authored) {
        const auto &decoration = theme.decoration;
        style.buttonSide = decoration.buttonPlacement == QLatin1String("left")
            ? ButtonSide::Left : ButtonSide::Right;
        style.tabDirection = decoration.tabDirection == QLatin1String("right-to-left")
            ? TabVisualDirection::RightToLeft : TabVisualDirection::LeftToRight;
        style.buttonStyle = decoration.buttonStyle == QLatin1String("traffic-lights")
            ? ButtonStyle::TrafficLights : ButtonStyle::Symbols;
        themeHoverGlyphs = decoration.hoverGlyphs;
    }
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

QVariantMap containerStyleToVariantMap(const ChromeStyle &style)
{
    const auto &palette = style.palette;
    return {
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
    return style;
}

} // namespace QindaQt::Decoration
