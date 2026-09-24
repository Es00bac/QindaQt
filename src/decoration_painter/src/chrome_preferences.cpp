// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include "qindaqt/decoration_painter/decoration_button_style.h"
#include "qindaqt/themes/decoration_theme_loader.h"

#include <utility>

// User arrangement for the two chrome sets (ADR-0129): the preference model,
// its Settings1 tokens, and the resolution of a theme plus preferences into
// the published window chrome. Container chrome resolves in
// container_chrome_style.cpp from the same preferences.
namespace QindaQt::Decoration {
namespace {

struct PreferenceField final {
    QLatin1String key;
    QString ChromePreferences::*member;
    QStringList tokens; // default first
};

const QList<PreferenceField> &preferenceFields()
{
    static const QList<PreferenceField> fields = [] {
        const auto theme = QStringLiteral("theme");
        // AGENT-CONTRACT: these lists mirror the allowedValues of the same
        // keys in data/settings/schema-v2.json; a token missing on either
        // side makes an Appearance commit fail validation. Both chrome sets
        // offer every named button style (ADR-0264); "symbols" stays a
        // theme-only name because windows paint it exactly like the lights.
        const QStringList buttonStyles{
            theme, QStringLiteral("traffic-lights"), QStringLiteral("flat"),
            QStringLiteral("glyph"), QStringLiteral("gel"), QStringLiteral("bevel"),
            QStringLiteral("blue-tiles"), QStringLiteral("wide"), QStringLiteral("tab"),
            QStringLiteral("bold"), QStringLiteral("minimal"), QStringLiteral("pills"),
            QStringLiteral("dots"), QStringLiteral("outline"), QStringLiteral("chunky")};
        const QStringList sizes{theme, QStringLiteral("small"), QStringLiteral("large")};
        const QStringList spacings{theme, QStringLiteral("tight"), QStringLiteral("roomy")};
        const QStringList switches{QStringLiteral("hidden"), QStringLiteral("shown")};
        const QStringList doubleClicks{QStringLiteral("maximize"), QStringLiteral("roll-up"),
                                       QStringLiteral("minimize")};
        return QList<PreferenceField>{
            {ChromePreferenceKeys::WindowButtonStyle, &ChromePreferences::windowButtonStyle,
             buttonStyles},
            {ChromePreferenceKeys::WindowButtonSide, &ChromePreferences::windowButtonSide,
             {theme, QStringLiteral("left"), QStringLiteral("right")}},
            {ChromePreferenceKeys::WindowButtons, &ChromePreferences::windowButtons,
             {QStringLiteral("all"), QStringLiteral("minimize-close"), QStringLiteral("close")}},
            {ChromePreferenceKeys::WindowTitleAlignment, &ChromePreferences::windowTitleAlignment,
             {QStringLiteral("center"), QStringLiteral("left")}},
            {ChromePreferenceKeys::ContainerButtonStyle, &ChromePreferences::containerButtonStyle,
             buttonStyles},
            {ChromePreferenceKeys::ContainerButtonSide, &ChromePreferences::containerButtonSide,
             {theme, QStringLiteral("left"), QStringLiteral("right")}},
            {ChromePreferenceKeys::ContainerTabOrder, &ChromePreferences::containerTabOrder,
             {theme, QStringLiteral("left-to-right"), QStringLiteral("right-to-left")}},
            {ChromePreferenceKeys::ContainerButtonGlyphs,
             &ChromePreferences::containerButtonGlyphs,
             {theme, QStringLiteral("always"), QStringLiteral("hover")}},
            // Title-bar options (ADR-0264), appended so the commit order of
            // the original eight keys never changes.
            {ChromePreferenceKeys::WindowButtonSize, &ChromePreferences::windowButtonSize, sizes},
            {ChromePreferenceKeys::WindowButtonSpacing, &ChromePreferences::windowButtonSpacing,
             spacings},
            {ChromePreferenceKeys::WindowTitleHeight, &ChromePreferences::windowTitleHeight,
             {theme, QStringLiteral("compact"), QStringLiteral("tall")}},
            {ChromePreferenceKeys::WindowCornerRadius, &ChromePreferences::windowCornerRadius,
             {theme, QStringLiteral("square"), QStringLiteral("small"), QStringLiteral("large")}},
            {ChromePreferenceKeys::WindowTitleWeight, &ChromePreferences::windowTitleWeight,
             {theme, QStringLiteral("regular"), QStringLiteral("bold")}},
            {ChromePreferenceKeys::WindowAppIcon, &ChromePreferences::windowAppIcon, switches},
            {ChromePreferenceKeys::WindowRollUpButton, &ChromePreferences::windowRollUpButton,
             switches},
            {ChromePreferenceKeys::WindowTitleDoubleClick,
             &ChromePreferences::windowTitleDoubleClick, QStringList{theme} + doubleClicks},
            {ChromePreferenceKeys::ContainerButtonSize, &ChromePreferences::containerButtonSize,
             sizes},
            {ChromePreferenceKeys::ContainerButtonSpacing,
             &ChromePreferences::containerButtonSpacing, spacings},
            {ChromePreferenceKeys::ContainerTitleDoubleClick,
             &ChromePreferences::containerTitleDoubleClick,
             QStringList{QStringLiteral("none")} + doubleClicks},
        };
    }();
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
    // Title-bar options (ADR-0264). "theme" keeps what the theme and its
    // decoration document resolved, so untouched chrome stays byte-identical.
    if (preferences.windowButtonSize == QLatin1String("small")) {
        chrome.buttonScale = 0.8;
    } else if (preferences.windowButtonSize == QLatin1String("large")) {
        chrome.buttonScale = 1.25;
    }
    if (preferences.windowButtonSpacing == QLatin1String("tight")) {
        chrome.spacingScale = 0.5;
    } else if (preferences.windowButtonSpacing == QLatin1String("roomy")) {
        chrome.spacingScale = 1.5;
    }
    // Heights are relative to the chosen style's own bar, so "tall" on the
    // chunky style is still taller than chunky's own.
    const auto &style = decorationButtonStyle(chrome.buttonStyle);
    const qreal ownHeight = style.titleHeight > 0.0 ? style.titleHeight : DecorationTitleHeight;
    if (preferences.windowTitleHeight == QLatin1String("compact")) {
        chrome.titleHeight = ownHeight - 4.0;
    } else if (preferences.windowTitleHeight == QLatin1String("tall")) {
        chrome.titleHeight = ownHeight + 6.0;
    }
    if (preferences.windowCornerRadius == QLatin1String("square")) {
        chrome.cornerRadius = 0.0;
    } else if (preferences.windowCornerRadius == QLatin1String("small")) {
        chrome.cornerRadius = 4.0;
    } else if (preferences.windowCornerRadius == QLatin1String("large")) {
        chrome.cornerRadius = 16.0;
    }
    if (preferences.windowTitleWeight == QLatin1String("regular")) {
        chrome.titleWeight = QFont::Normal;
    } else if (preferences.windowTitleWeight == QLatin1String("bold")) {
        chrome.titleWeight = QFont::Bold;
    }
    chrome.appIcon = preferences.windowAppIcon == QLatin1String("shown");
    chrome.rollUpButton = preferences.windowRollUpButton == QLatin1String("shown");
    if (preferences.windowTitleDoubleClick != QLatin1String("theme")) {
        chrome.titleDoubleClick = preferences.windowTitleDoubleClick;
    }
    return chrome;
}

DecorationChrome resolveWindowChrome(const Themes::ThemeSpec &theme,
                                     const ChromePreferences &preferences)
{
    return applyWindowPreferences(DecorationChrome::fromTheme(theme), preferences);
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

} // namespace QindaQt::Decoration
