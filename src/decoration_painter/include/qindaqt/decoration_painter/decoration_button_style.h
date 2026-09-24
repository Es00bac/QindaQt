// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QColor>
#include <QList>
#include <QRectF>
#include <QSizeF>
#include <QString>

class QPainter;

namespace QindaQt::HybridChrome {
struct ChromeRenderPlan;
struct WindowButtonGeometry;
} // namespace QindaQt::HybridChrome

// Window-button styles as data (ADR-0264). One painter draws every button
// from a DecorationButtonStyle row, windows and containers alike. The four
// pre-W19 styles are rows too and reproduce their old pixels, except that
// the glyph set's strokes are now solid instead of a dashed "worn" pen.
namespace QindaQt::Decoration {

// The plate a button sits on. None draws no plate at rest; Dot is a small
// dot that grows into a circle while the glyphs show; Pill joins the whole
// cluster into one capsule and is painted by the Plate hover only.
enum class DecorationButtonShape { None, Circle, RoundedSquare, Square, Pill, Dot };
// How a resting (Tint) plate is filled.
enum class DecorationButtonFill { Solid, Gradient, Glass, Outline, Bevel };
// Where a plate's color comes from: the chrome's action colors, the title
// surface, or the style's own face colors.
enum class DecorationButtonColors { Action, Surface, Own };
// Hover and press: Tint lightens or darkens a resting plate; Plate shows a
// caption-ink plate under the pointer (the close may take its own color);
// Halo glows behind a plate-less glyph.
enum class DecorationButtonHover { Tint, Plate, Halo };
// Glyph drawings: Classic is the traffic-light set, Lines the monochrome
// flat set, Console the console-style set (close cross, minimize circle,
// maximize triangle, restore square).
enum class DecorationGlyphFamily { Classic, Lines, Console };
// Glyph ink: black or white against the plate, the caption ink, or the
// action color itself.
enum class DecorationGlyphInk { Contrast, Caption, Action };

// AGENT-NOTE: every member has a default initializer so the style table can
// use designated initializers without -Wmissing-field-initializers.
struct DecorationButtonStyle final {
    QString name{};
    DecorationButtonShape shape = DecorationButtonShape::Circle;
    DecorationButtonFill fill = DecorationButtonFill::Solid;
    DecorationButtonColors colors = DecorationButtonColors::Action;
    DecorationButtonHover hover = DecorationButtonHover::Tint;
    DecorationGlyphFamily glyphs = DecorationGlyphFamily::Classic;
    DecorationGlyphInk ink = DecorationGlyphInk::Contrast;
    // Glyphs show only while a control is hovered (traffic lights).
    bool glyphsOnHover = false;
    // Plate hover: the close button takes the close color under the pointer.
    bool closeHoverColor = false;
    // Plate hover: the caption-ink plate's alpha at rest; 0 draws none.
    int restAlpha = 0;
    // Glyph pen width and the corner radius of rounded plates.
    qreal stroke = 1.3;
    qreal radius = 3.0;
    // Cell height (0 fills the title bar), width over height, and the gap.
    qreal size = 16.0;
    qreal aspect = 1.0;
    qreal spacing = 8.0;
    // The cluster touches the title-bar edge instead of the usual insets.
    bool flush = false;
    // The style's own title-bar height; 0 keeps DecorationTitleHeight.
    qreal titleHeight = 0.0;
    // The side when neither the theme nor the user names one.
    DecorationButtonSide side = DecorationButtonSide::Left;
    // Own plate colors (colors == Own); the close face may differ.
    QColor face{};
    QColor closeFace{};
    // The title bar is a tab that ends after the caption (BeOS-like).
    bool titleTab = false;
};

// Every style, in the order Appearance lists them after "theme".
[[nodiscard]] const QList<DecorationButtonStyle> &decorationButtonStyles();
// The style a name selects. An unknown name falls back to the traffic
// lights, which is what any unknown style name painted before ADR-0264.
[[nodiscard]] const DecorationButtonStyle &decorationButtonStyle(const QString &name);
[[nodiscard]] bool isDecorationButtonStyle(const QString &name);

// One button handed to the style painter.
struct DecorationStyledButton final {
    DecorationButtonKind kind = DecorationButtonKind::Close;
    QRectF geometry;
    bool active = true;
    bool hovered = false;
    bool pressed = false;
    bool glyphVisible = true;
    bool restoreGlyph = false;
    // Position in the cluster, for joined (Pill) plates.
    qsizetype index = 0;
    qsizetype count = 1;
};

// Paints one button: its plate, hover treatment, and glyph. The caller
// turns antialiasing on and saves and restores the painter around it.
void paintStyledButton(QPainter &painter, const DecorationButtonStyle &style,
                       const DecorationChrome &chrome, const DecorationStyledButton &button);

// The cell and gap the window layout gives the chrome's style under its
// size and spacing options.
[[nodiscard]] QSizeF decorationButtonCell(const DecorationChrome &chrome);
[[nodiscard]] qreal decorationButtonGap(const DecorationChrome &chrome);

// The HybridChrome::ChromeButtonPainter containers use (ADR-0264): the
// plan's palette stands in for the chrome and plan.style.namedButtonStyle
// names the style, so a container button matches a window button.
void paintContainerButton(QPainter &painter, const HybridChrome::ChromeRenderPlan &plan,
                          const HybridChrome::WindowButtonGeometry &button, bool hovered,
                          bool pressed, bool glyphVisible);

} // namespace QindaQt::Decoration
