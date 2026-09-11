// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"
#include "qindaqt/themes/theme_spec.h"

#include <QColor>
#include <QFont>
#include <QLatin1String>
#include <QList>
#include <QStringList>
#include <QMarginsF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVariantMap>

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

enum class DecorationButtonKind { Close, Minimize, Maximize };
enum class DecorationButtonSide { Left, Right };

struct DecorationButtonVisual {
    DecorationButtonKind kind = DecorationButtonKind::Close;
    QRectF geometry;
    bool visible = true;
    bool hovered = false;
    bool pressed = false;
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
};

[[nodiscard]] DecorationVisualStyle decorationVisualStyle(const QColor &border,
                                                          const QColor &surface,
                                                          bool maximized);
[[nodiscard]] QMarginsF decorationBorders(bool maximized);
[[nodiscard]] QMarginsF decorationResizeOnlyBorders(bool maximized,
                                                    bool containerMember);
// Button geometry exactly as the live decoration lays it out: classic
// symbols on the physical left (close, minimize, maximize), Luna glyphs on
// the physical right (minimize, maximize, close).
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

// Arrangement (ADR-0129): an explicit buttonSide wins; otherwise glyph and
// flat buttons sit on the right and every other style on the left.
[[nodiscard]] DecorationButtonSide effectiveButtonSide(const DecorationChrome &chrome);
// Visible actions in physical left-to-right order for the resolved side and
// button set: the right edge reads minimize, maximize, close; the left edge
// keeps Qinda macOS logical order close, minimize, maximize.
[[nodiscard]] QList<DecorationButtonKind>
decorationButtonKinds(const DecorationChrome &chrome);

// Stable Settings1 keys for the two chrome sets (ADR-0129). They mirror
// data/settings/schema-v2.json and are part of the Appearance route scope.
namespace ChromePreferenceKeys {
inline constexpr QLatin1String WindowButtonStyle{"appearance.windowButtonStyle"};
inline constexpr QLatin1String WindowButtonSide{"appearance.windowButtonSide"};
inline constexpr QLatin1String WindowButtons{"appearance.windowButtons"};
inline constexpr QLatin1String WindowTitleAlignment{"appearance.windowTitleAlignment"};
inline constexpr QLatin1String ContainerButtonStyle{"appearance.containerButtonStyle"};
inline constexpr QLatin1String ContainerButtonSide{"appearance.containerButtonSide"};
inline constexpr QLatin1String ContainerTabOrder{"appearance.containerTabOrder"};
inline constexpr QLatin1String ContainerButtonGlyphs{"appearance.containerButtonGlyphs"};
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

    [[nodiscard]] static QStringList settingsKeys();
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
[[nodiscard]] QVariantMap containerStyleToVariantMap(const HybridChrome::ChromeStyle &style);
// Tolerant: absent or mistyped keys keep ChromeStyle's defaults.
[[nodiscard]] HybridChrome::ChromeStyle containerStyleFromVariantMap(const QVariantMap &map);

} // namespace QindaQt::Decoration
