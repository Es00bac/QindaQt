// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"
#include "qindaqt/themes/decoration_theme_spec.h"
#include "qindaqt/themes/theme_spec.h"

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QLatin1String>
#include <QList>
#include <QStringList>
#include <QMarginsF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVariantMap>

#include <optional>

class QPainter;

// AGENT-CONTRACT: The one renderer for QindaQt window chrome (ADR-0127).
// The KDecoration plugin paints live windows through these functions, and
// the Settings Appearance preview paints the same functions into an item, so
// a preview can never drift from what the compositor draws. Nothing here
// touches KDecoration, KWin, or a service client: inputs are a resolved
// DecorationChrome, a frame state, and a painter.
namespace QindaQt::Decoration {

constexpr qreal DecorationTitleHeight = 24.0;
constexpr qreal DecorationCornerRadius = 10.0;
constexpr qreal DecorationClassicButtonSize = 14.0;
constexpr qreal DecorationGlyphButtonSize = 16.0;
constexpr qreal DecorationButtonSpacing = 8.0;
// Contained windows (ADR-0131): container members keep a compact handlebar
// instead of a full title bar, tall enough to grab, with miniature stoplight
// controls in 12 px hit cells and a quieter corner radius.
constexpr qreal DecorationMemberHandleHeight = 14.0;
constexpr qreal DecorationMemberCornerRadius = 6.0;
constexpr qreal DecorationMiniButtonCell = 12.0;
constexpr qreal DecorationMiniButtonSpacing = 3.0;
constexpr qreal DecorationMiniButtonInset = 6.0;
constexpr qreal DecorationMemberGripMaximumWidth = 36.0;
constexpr qreal DecorationMemberGripMinimumWidth = 8.0;
constexpr qreal DecorationMemberControlClearance = 2.0;
// AGENT-CONTRACT: Three stoplights at one end, More at the other, and a
// centered minimum grip fit without overlap at this width. Container sizing
// must treat a layout below it as explicitly unsupported.
constexpr qreal DecorationMemberHandleMinimumWidth = 108.0;

struct DecorationVisualStyle {
    QColor frameColor;
    QColor shadowColor;
    qreal frameWidth = 1.0;
    qreal cornerRadius = DecorationCornerRadius;
    qreal shadowExtent = 12.0;
    qreal shadowOpacity = 0.30;
    bool framed = true;
};

// The flattened chrome the compositor publishes to every decoration as the
// `qindaqtChromePalette` dynamic property. Invalid authored colors mean
// "not authored": the classic rendering stays byte-identical.
struct DecorationChrome {
    QColor surface;
    QColor surfaceRaised;
    QColor border;
    QColor text;
    QColor textMuted;
    QColor close;
    QColor minimize;
    QColor maximize;
    QString buttonStyle = QStringLiteral("symbols");
    QColor titleBar;
    QColor titleBarInactive;
    QColor restore;
    // Arrangement (ADR-0129). An empty side keeps the pre-preference rule
    // (glyph and flat buttons right, every other style left); `buttons` is
    // all, minimize-close, or close; `titleAlignment` is center or left.
    // Defaults are omitted from the published map, so untouched chrome stays
    // byte-identical to what shipped before these fields existed.
    QString buttonSide;
    QString buttons = QStringLiteral("all");
    QString titleAlignment = QStringLiteral("center");
    // Container identity emphasis (ADR-0139). `identityColor` is the owning
    // container's color and `memberFocused` marks the one focused member;
    // both arrive only for contained windows. An invalid color / false flag
    // is omitted from the published map, so neutral members keep today's
    // handlebar byte-identical.
    QColor identityColor;
    bool memberFocused = false;
    // Theming v2 material (ADR-0207). Every default reproduces the shipped
    // chrome, and toVariantMap omits defaults, so untouched themes publish a
    // byte-identical map.
    double titleOpacity = 1.0;
    bool titleBlur = false;
    bool titleHighlight = false;
    QColor titleTint;
    double cornerRadius = DecorationCornerRadius;
    double shadowExtent = 12.0;
    double shadowOpacity = 0.30;
    // grip, dots, or plain (contained-window handlebar, ADR-0131).
    QString handleStyle = QStringLiteral("grip");
    // Title-bar options (ADR-0264). Every default reproduces the shipped
    // chrome, and toVariantMap omits defaults.
    // Button cell and gap scales: the size and spacing options.
    double buttonScale = 1.0;
    double spacingScale = 1.0;
    // Title-bar height; 0 keeps the button style's own (24 for most).
    double titleHeight = 0.0;
    // Caption weight as a QFont::Weight value; 0 keeps the shipped weight
    // (DemiBold, Bold on the worn Luna bar).
    int titleWeight = 0;
    // Paint the window's application icon in front of the caption.
    bool appIcon = false;
    // Offer the roll-up button, the compositor's roll-up (ADR-0203).
    bool rollUpButton = false;
    // Title-bar double-click: empty leaves KWin's own action; otherwise
    // maximize, roll-up, or minimize, which the decoration runs itself.
    QString titleDoubleClick;

    [[nodiscard]] static DecorationChrome
    fromChromePalette(const HybridChrome::ChromePalette &palette,
                      const Themes::ThemeSpec &theme);
    [[nodiscard]] static DecorationChrome fromTheme(const Themes::ThemeSpec &theme);
    // Tolerant: absent or mistyped keys keep the defaults, never fail.
    [[nodiscard]] static DecorationChrome fromVariantMap(const QVariantMap &map);
    [[nodiscard]] QVariantMap toVariantMap() const;

    [[nodiscard]] bool glyphChrome() const;
    [[nodiscard]] bool flatChrome() const;
    [[nodiscard]] bool wornLuna() const;
};

// The compositor's theme -> chrome palette derivation, shared so the preview
// derives the identical palette from a ThemeSpec.
[[nodiscard]] HybridChrome::ChromePalette
chromePaletteForTheme(const Themes::ThemeSpec &theme);

// More opens the window menu; it exists only on contained-window handlebars.
// RollUp rolls an independent window up to its icon (ADR-0203, ADR-0264).
enum class DecorationButtonKind { Close, Minimize, Maximize, More, RollUp };
enum class DecorationButtonSide { Left, Right };

struct DecorationButtonVisual {
    DecorationButtonKind kind = DecorationButtonKind::Close;
    QRectF geometry;
    bool visible = true;
    bool hovered = false;
    bool pressed = false;
};

// Value-only geometry shared by the preview painter and live KDecoration.
// `dragRegion` is the button-free native detach target; supported guarantees
// every control and the centered grip are in bounds and mutually disjoint.
struct DecorationMemberHandleLayout {
    QList<DecorationButtonVisual> buttons;
    QRectF grip;
    QRectF dragRegion;
    bool supported = false;
};

struct DecorationFrameVisual {
    QSizeF size;
    QString caption;
    QFont font;
    bool active = true;
    bool maximized = false;
    // Classic buttons reveal their glyphs only while any control is hovered.
    bool controlsHovered = false;
    // Maximize draws the restore glyph (window maximized or focus-maximized).
    bool restoreGlyph = false;
    // Paint the contained-window handlebar instead of the full title bar.
    bool memberHandle = false;
    // ADR-0139: this member is its container's focused member, so the
    // handlebar wears the chrome identity color. Carried on the frame (not
    // the chrome) because it is per-window live state, not theme data.
    bool memberFocused = false;
    // The window's application icon, painted when chrome.appIcon is set.
    QIcon icon;
};

[[nodiscard]] DecorationVisualStyle decorationVisualStyle(const QColor &border,
                                                          const QColor &surface,
                                                          bool maximized);
// The same style with the chrome's authored radius and shadow (ADR-0207).
[[nodiscard]] DecorationVisualStyle decorationVisualStyleFor(const DecorationChrome &chrome,
                                                             bool maximized);
// The theme-scaled corner radius the title and frame paint with: the chrome's
// authored radius for a floating window, zero when maximized.
[[nodiscard]] qreal decorationFrameRadius(const DecorationChrome &chrome, bool maximized);
[[nodiscard]] QMarginsF decorationBorders(bool maximized);
// The title-bar height the chrome paints and the plugin reserves: the
// height option, else the button style's own, else DecorationTitleHeight.
[[nodiscard]] qreal decorationTitleHeight(const DecorationChrome &chrome);
[[nodiscard]] QMarginsF decorationBorders(const DecorationChrome &chrome, bool maximized);
[[nodiscard]] QMarginsF decorationResizeOnlyBorders(bool maximized,
                                                    bool containerMember);
// Button geometry exactly as the live decoration lays it out: the style's
// cell, gap and edge inset (ADR-0264) under the size and spacing options,
// centered in the title bar, on the side effectiveButtonSide resolves.
[[nodiscard]] QList<DecorationButtonVisual>
layoutDecorationButtons(const DecorationChrome &chrome, const QSizeF &size);
[[nodiscard]] QRectF decorationCaptionRect(const DecorationChrome &chrome,
                                           const QSizeF &size,
                                           const QList<DecorationButtonVisual> &buttons);

[[nodiscard]] QColor decorationTitleColor(const DecorationChrome &chrome, bool active);
[[nodiscard]] QColor decorationCaptionColor(const DecorationChrome &chrome, bool active);
[[nodiscard]] QColor decorationTextColor(const DecorationChrome &chrome, bool active);
[[nodiscard]] QColor decorationButtonFill(const DecorationChrome &chrome,
                                          DecorationButtonKind kind, bool active);
[[nodiscard]] QColor decorationButtonGlyphColor(const DecorationChrome &chrome,
                                                DecorationButtonKind kind, bool active);
[[nodiscard]] QColor decorationGlyphChromeColor(const DecorationChrome &chrome,
                                                DecorationButtonKind kind,
                                                bool restoreGlyph);
[[nodiscard]] quint32 decorationWearSeed(const QString &caption, qreal width);

void paintWornLunaTitle(QPainter &painter, const QRectF &bar,
                        const QColor &paintColor, quint32 seed);
void paintDecorationFrame(QPainter &painter, const QRectF &bounds,
                          const DecorationVisualStyle &style);
// Title fill (worn or classic), seam line, and frame outline.
void paintDecorationTitle(QPainter &painter, const DecorationChrome &chrome,
                          const DecorationFrameVisual &frame);
void paintDecorationButton(QPainter &painter, const DecorationChrome &chrome,
                           const DecorationFrameVisual &frame,
                           const DecorationButtonVisual &button);
void paintDecorationCaption(QPainter &painter, const DecorationChrome &chrome,
                            const DecorationFrameVisual &frame,
                            const QRectF &captionRect);
// The complete chrome in one call: title, frame, buttons, caption.
void paintDecoration(QPainter &painter, const DecorationChrome &chrome,
                     const DecorationFrameVisual &frame,
                     const QList<DecorationButtonVisual> &buttons);

// Arrangement (ADR-0129): an explicit buttonSide wins; otherwise the button
// style's own side (glyph and flat right, the lights left, ADR-0264).
[[nodiscard]] DecorationButtonSide effectiveButtonSide(const DecorationChrome &chrome);
// Visible actions in physical left-to-right order for the resolved side and
// button set: the right edge reads minimize, maximize, close; the left edge
// keeps Qinda macOS logical order close, minimize, maximize. The roll-up
// button (ADR-0264) sits at the inner end of either.
[[nodiscard]] QList<DecorationButtonKind>
decorationButtonKinds(const DecorationChrome &chrome);

// Stable Settings1 keys for the two chrome sets (ADR-0129). They mirror
// data/settings/schema-v2.json and are part of the Appearance route scope.
namespace ChromePreferenceKeys {
// Decoration theme document ids for windows and containers (ADR-0207);
// "theme" follows the color theme's own pairing. Scoped by their own
// purpose-scoped client, never added to settingsKeys(): an older resident
// Settings1 that does not know them must cost only the decoration choice.
inline constexpr QLatin1String WindowDecoration{"appearance.windowDecoration"};
inline constexpr QLatin1String ContainerDecoration{"appearance.containerDecoration"};
inline constexpr QLatin1String WindowButtonStyle{"appearance.windowButtonStyle"};
inline constexpr QLatin1String WindowButtonSide{"appearance.windowButtonSide"};
inline constexpr QLatin1String WindowButtons{"appearance.windowButtons"};
inline constexpr QLatin1String WindowTitleAlignment{"appearance.windowTitleAlignment"};
inline constexpr QLatin1String ContainerButtonStyle{"appearance.containerButtonStyle"};
inline constexpr QLatin1String ContainerButtonSide{"appearance.containerButtonSide"};
inline constexpr QLatin1String ContainerTabOrder{"appearance.containerTabOrder"};
inline constexpr QLatin1String ContainerButtonGlyphs{"appearance.containerButtonGlyphs"};
// Title-bar options (ADR-0264).
inline constexpr QLatin1String WindowButtonSize{"appearance.windowButtonSize"};
inline constexpr QLatin1String WindowButtonSpacing{"appearance.windowButtonSpacing"};
inline constexpr QLatin1String WindowTitleHeight{"appearance.windowTitleHeight"};
inline constexpr QLatin1String WindowCornerRadius{"appearance.windowCornerRadius"};
inline constexpr QLatin1String WindowTitleWeight{"appearance.windowTitleWeight"};
inline constexpr QLatin1String WindowAppIcon{"appearance.windowAppIcon"};
inline constexpr QLatin1String WindowRollUpButton{"appearance.windowRollUpButton"};
inline constexpr QLatin1String WindowTitleDoubleClick{"appearance.windowTitleDoubleClick"};
inline constexpr QLatin1String ContainerButtonSize{"appearance.containerButtonSize"};
inline constexpr QLatin1String ContainerButtonSpacing{"appearance.containerButtonSpacing"};
inline constexpr QLatin1String ContainerTitleDoubleClick{"appearance.containerTitleDoubleClick"};
} // namespace ChromePreferenceKeys

// The user's arrangement for application window decorations and container
// chrome. "theme" defers to the resolved theme, and every default reproduces
// the chrome that shipped before these preferences existed.
struct ChromePreferences {
    QString windowButtonStyle = QStringLiteral("theme");
    QString windowButtonSide = QStringLiteral("theme");
    QString windowButtons = QStringLiteral("all");
    QString windowTitleAlignment = QStringLiteral("center");
    QString containerButtonStyle = QStringLiteral("theme");
    QString containerButtonSide = QStringLiteral("theme");
    QString containerTabOrder = QStringLiteral("theme");
    QString containerButtonGlyphs = QStringLiteral("theme");
    // Decoration document ids (ADR-0207): "theme" or a document id.
    QString windowDecoration = QStringLiteral("theme");
    QString containerDecoration = QStringLiteral("theme");
    // Title-bar options (ADR-0264). "theme" keeps what the theme and its
    // decoration document resolved; the window double-click "theme" leaves
    // KWin's own action and the container "none" keeps the row inert.
    QString windowButtonSize = QStringLiteral("theme");
    QString windowButtonSpacing = QStringLiteral("theme");
    QString windowTitleHeight = QStringLiteral("theme");
    QString windowCornerRadius = QStringLiteral("theme");
    QString windowTitleWeight = QStringLiteral("theme");
    QString windowAppIcon = QStringLiteral("hidden");
    QString windowRollUpButton = QStringLiteral("hidden");
    QString windowTitleDoubleClick = QStringLiteral("theme");
    QString containerButtonSize = QStringLiteral("theme");
    QString containerButtonSpacing = QStringLiteral("theme");
    QString containerTitleDoubleClick = QStringLiteral("none");

    [[nodiscard]] static QStringList settingsKeys();
    // The two decoration-document keys, scoped separately (see above).
    [[nodiscard]] static QStringList decorationKeys();
    // Allowed tokens for one key, default first; empty for an unknown key.
    [[nodiscard]] static QStringList tokens(const QString &key);
    // Tolerant: a missing, mistyped, or unknown value keeps its default.
    [[nodiscard]] static ChromePreferences fromSettingsValues(const QVariantMap &values);
    [[nodiscard]] QVariantMap toSettingsValues() const;
    [[nodiscard]] QString token(const QString &key) const;
    // Returns false and leaves the field unchanged for an unknown key or token.
    bool setToken(const QString &key, const QString &token);
    [[nodiscard]] bool operator==(const ChromePreferences &) const = default;
};

[[nodiscard]] DecorationChrome applyWindowPreferences(DecorationChrome chrome,
                                                      const ChromePreferences &preferences);
[[nodiscard]] DecorationChrome resolveWindowChrome(const Themes::ThemeSpec &theme,
                                                   const ChromePreferences &preferences);
// Container chrome follows a theme that authors a `decoration` block and
// keeps the Qinda macOS arrangement otherwise, then applies preferences.
[[nodiscard]] HybridChrome::ChromeStyle
resolveContainerStyle(const Themes::ThemeSpec &theme, const ChromePreferences &preferences);

// Theming v2 (ADR-0207): a decoration theme document layered over the color
// theme. The document's arrangement, material, radius, shadow, and handle
// style replace the theme's; its colors apply only where authored, so a
// neutral document keeps the palette's own button colors.
[[nodiscard]] DecorationChrome applyDecorationTheme(DecorationChrome chrome,
                                                    const Themes::DecorationThemeSpec &document);
[[nodiscard]] HybridChrome::ChromeStyle
applyDecorationTheme(HybridChrome::ChromeStyle style,
                     const Themes::DecorationThemeSpec &document);
// The document a preference token selects: the named document when it is
// installed, else the color theme's own `decorationTheme` pairing, else none.
// "theme" (or any unknown id) means "follow the color theme".
[[nodiscard]] std::optional<Themes::DecorationThemeSpec>
selectDecorationTheme(const Themes::ThemeSpec &theme,
                      const QVector<Themes::DecorationThemeSpec> &installed,
                      const QString &preference);
// Full resolution: theme, optional document, then the user's arrangement.
[[nodiscard]] DecorationChrome
resolveWindowChrome(const Themes::ThemeSpec &theme,
                    const std::optional<Themes::DecorationThemeSpec> &document,
                    const ChromePreferences &preferences);
// The same layering over an already-derived chrome (the compositor derives
// its window chrome from the shared chrome palette): the theme's own v2
// decoration surface, then the document, then the user's arrangement.
[[nodiscard]] DecorationChrome
decorateWindowChrome(DecorationChrome chrome, const Themes::ThemeSpec &theme,
                     const std::optional<Themes::DecorationThemeSpec> &document,
                     const ChromePreferences &preferences);
[[nodiscard]] HybridChrome::ChromeStyle
resolveContainerStyle(const Themes::ThemeSpec &theme,
                      const std::optional<Themes::DecorationThemeSpec> &document,
                      const ChromePreferences &preferences);
// The color theme's own material for container chrome (schema v2 surfaces),
// applied by the resolvers above before any document.
[[nodiscard]] HybridChrome::ChromeMaterial
containerMaterialForTheme(const Themes::ThemeSpec &theme);
[[nodiscard]] QVariantMap containerStyleToVariantMap(const HybridChrome::ChromeStyle &style);
// Tolerant: absent or mistyped keys keep ChromeStyle's defaults.
[[nodiscard]] HybridChrome::ChromeStyle containerStyleFromVariantMap(const QVariantMap &map);

// Contained-window handlebar (ADR-0131): stoplights on the effective button
// side with the visible set applied, and a "more" control that opens the
// window menu at the opposite end, all in miniature hit cells.
[[nodiscard]] DecorationMemberHandleLayout
layoutMemberHandle(const DecorationChrome &chrome, const QSizeF &size);
// Compatibility projection for callers that paint only the controls. New
// geometry consumers should retain the complete layout above.
[[nodiscard]] QList<DecorationButtonVisual>
layoutMemberHandleButtons(const DecorationChrome &chrome, const QSizeF &size);
// Handle bar, seam, frame outline, and the centered grip. Its buttons paint
// through paintDecorationButton with frame.memberHandle set.
void paintMemberHandle(QPainter &painter, const DecorationChrome &chrome,
                       const DecorationFrameVisual &frame);
// Identity emphasis for the focused member's handlebar (ADR-0139): the fill
// is the container identity color when this member is focused, else the
// neutral title surface; the ink (grip, "more" dots) switches light/dark to
// hold 4.5:1 against that fill.
[[nodiscard]] QColor decorationMemberHandleFillColor(const DecorationChrome &chrome,
                                                     const DecorationFrameVisual &frame);
[[nodiscard]] QColor decorationMemberHandleInkColor(const DecorationChrome &chrome,
                                                    const DecorationFrameVisual &frame);

} // namespace QindaQt::Decoration
