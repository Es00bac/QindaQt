// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_button_style.h"

#include <QtMath>

#include <algorithm>

// The named window-button styles (ADR-0264) and the metrics they imply.
// Each row is data for the one painter in decoration_button_painter.cpp.
// A new style is a new row here plus its name in
// Themes::DecorationThemeTokens::buttonStyles(), the two Settings1 token
// lists (chrome_preferences.cpp, data/settings/schema-v2.json), and the
// Appearance menu (AppearanceWindowsSection.qml).
namespace QindaQt::Decoration {
namespace {

using Colors = DecorationButtonColors;
using Fill = DecorationButtonFill;
using Glyphs = DecorationGlyphFamily;
using Hover = DecorationButtonHover;
using Ink = DecorationGlyphInk;
using Shape = DecorationButtonShape;
using Side = DecorationButtonSide;

QList<DecorationButtonStyle> buildStyles()
{
    // AGENT-GUARD: the first four rows are the pre-W19 painters (classic
    // lights twice, console glyphs, flat symbols). Changing their shape,
    // hover, glyphs, ink, stroke, size, spacing or side repaints every
    // shipped theme; tst_decoration_button_styles pins them pixel for pixel
    // against a copy of the old painters.
    return {
        {.name = QStringLiteral("traffic-lights"), .shape = Shape::Circle, .fill = Fill::Solid,
         .colors = Colors::Action, .hover = Hover::Tint, .glyphs = Glyphs::Classic,
         .ink = Ink::Contrast, .glyphsOnHover = true, .stroke = 1.15, .size = 14.0,
         .spacing = 8.0, .side = Side::Left},
        {.name = QStringLiteral("symbols"), .shape = Shape::Circle, .fill = Fill::Solid,
         .colors = Colors::Action, .hover = Hover::Tint, .glyphs = Glyphs::Classic,
         .ink = Ink::Contrast, .glyphsOnHover = true, .stroke = 1.15, .size = 14.0,
         .spacing = 8.0, .side = Side::Left},
        {.name = QStringLiteral("glyph"), .shape = Shape::None, .hover = Hover::Halo,
         .glyphs = Glyphs::Console, .ink = Ink::Action, .stroke = 1.8, .size = 16.0,
         .spacing = 8.0, .side = Side::Right},
        {.name = QStringLiteral("flat"), .shape = Shape::None, .hover = Hover::Plate,
         .glyphs = Glyphs::Lines, .ink = Ink::Caption, .closeHoverColor = true, .stroke = 1.3,
         .radius = 3.0, .size = 16.0, .spacing = 8.0, .side = Side::Right},
        // W19 styles. All artwork is drawn here from shapes and our own
        // colors; none copies another desktop's assets (ADR-0264).
        // Gel: glossy circles in the action colors, glyphs on hover.
        {.name = QStringLiteral("gel"), .shape = Shape::Circle, .fill = Fill::Glass,
         .colors = Colors::Action, .hover = Hover::Tint, .glyphs = Glyphs::Classic,
         .ink = Ink::Contrast, .glyphsOnHover = true, .stroke = 1.2, .size = 15.0,
         .spacing = 7.0, .side = Side::Left},
        // Bevel: raised squares in the title color with outlined glyphs.
        {.name = QStringLiteral("bevel"), .shape = Shape::Square, .fill = Fill::Bevel,
         .colors = Colors::Surface, .hover = Hover::Tint, .glyphs = Glyphs::Lines,
         .ink = Ink::Caption, .stroke = 1.6, .size = 16.0, .spacing = 2.0,
         .side = Side::Right},
        // Blue tiles: rounded blue gradient tiles with a crimson close.
        {.name = QStringLiteral("blue-tiles"), .shape = Shape::RoundedSquare,
         .fill = Fill::Gradient, .colors = Colors::Own, .hover = Hover::Tint,
         .glyphs = Glyphs::Lines, .ink = Ink::Contrast, .stroke = 1.9, .radius = 4.5,
         .size = 18.0, .spacing = 3.0, .side = Side::Right,
         .face = QColor(0x2F, 0x7F, 0xC4), .closeFace = QColor(0xCF, 0x4A, 0x3F)},
        // Wide: full-height flat cells flush with the edge; the close cell
        // takes the close color under the pointer.
        {.name = QStringLiteral("wide"), .shape = Shape::None, .hover = Hover::Plate,
         .glyphs = Glyphs::Lines, .ink = Ink::Caption, .closeHoverColor = true, .stroke = 1.0,
         .radius = 0.0, .size = 0.0, .aspect = 1.5, .spacing = 0.0, .flush = true,
         .side = Side::Right},
        // Tab: small bevelled boxes on a title tab that ends after the caption.
        {.name = QStringLiteral("tab"), .shape = Shape::Square, .fill = Fill::Bevel,
         .colors = Colors::Surface, .hover = Hover::Tint, .glyphs = Glyphs::Lines,
         .ink = Ink::Caption, .stroke = 1.2, .size = 14.0, .spacing = 4.0,
         .side = Side::Left, .titleTab = true},
        // Bold: small light-grey bevelled squares with heavy black glyphs.
        {.name = QStringLiteral("bold"), .shape = Shape::Square, .fill = Fill::Bevel,
         .colors = Colors::Own, .hover = Hover::Tint, .glyphs = Glyphs::Lines,
         .ink = Ink::Contrast, .stroke = 2.2, .size = 15.0, .spacing = 4.0,
         .side = Side::Right, .face = QColor(0xD6, 0xD6, 0xD2),
         .closeFace = QColor(0xD6, 0xD6, 0xD2)},
        // Minimal: thin glyphs, no plates until the pointer arrives.
        {.name = QStringLiteral("minimal"), .shape = Shape::None, .hover = Hover::Plate,
         .glyphs = Glyphs::Lines, .ink = Ink::Caption, .stroke = 1.0, .radius = 3.0,
         .size = 14.0, .spacing = 12.0, .side = Side::Right},
        // Pills: one capsule holds the whole cluster.
        {.name = QStringLiteral("pills"), .shape = Shape::Pill, .hover = Hover::Plate,
         .glyphs = Glyphs::Lines, .ink = Ink::Caption, .closeHoverColor = true,
         .restAlpha = 28, .stroke = 1.2, .size = 18.0, .aspect = 1.35, .spacing = 0.0,
         .side = Side::Right},
        // Dots: tiny colored dots that grow into lights with glyphs on hover.
        {.name = QStringLiteral("dots"), .shape = Shape::Dot, .fill = Fill::Solid,
         .colors = Colors::Action, .hover = Hover::Tint, .glyphs = Glyphs::Classic,
         .ink = Ink::Contrast, .glyphsOnHover = true, .stroke = 1.15, .size = 14.0,
         .spacing = 6.0, .side = Side::Left},
        // Outline: thin rings in the action colors, glyphs in the ring color.
        {.name = QStringLiteral("outline"), .shape = Shape::Circle, .fill = Fill::Outline,
         .colors = Colors::Action, .hover = Hover::Tint, .glyphs = Glyphs::Classic,
         .ink = Ink::Action, .glyphsOnHover = true, .stroke = 1.2, .size = 14.0,
         .spacing = 8.0, .side = Side::Left},
        // Chunky: touch-sized rounded squares on a taller title bar.
        {.name = QStringLiteral("chunky"), .shape = Shape::RoundedSquare, .fill = Fill::Solid,
         .colors = Colors::Action, .hover = Hover::Tint, .glyphs = Glyphs::Lines,
         .ink = Ink::Contrast, .stroke = 2.2, .radius = 6.0, .size = 26.0, .spacing = 8.0,
         .titleHeight = 34.0, .side = Side::Right},
    };
}

} // namespace

const QList<DecorationButtonStyle> &decorationButtonStyles()
{
    static const QList<DecorationButtonStyle> styles = buildStyles();
    return styles;
}

const DecorationButtonStyle &decorationButtonStyle(const QString &name)
{
    const auto &styles = decorationButtonStyles();
    for (const auto &style : styles) {
        if (style.name == name) {
            return style;
        }
    }
    return styles.constFirst();
}

bool isDecorationButtonStyle(const QString &name)
{
    const auto &styles = decorationButtonStyles();
    return std::any_of(styles.cbegin(), styles.cend(),
                       [&name](const DecorationButtonStyle &style) { return style.name == name; });
}

qreal decorationTitleHeight(const DecorationChrome &chrome)
{
    if (chrome.titleHeight > 0.0) {
        return std::max(16.0, chrome.titleHeight);
    }
    const auto &style = decorationButtonStyle(chrome.buttonStyle);
    return style.titleHeight > 0.0 ? style.titleHeight : DecorationTitleHeight;
}

QMarginsF decorationBorders(const DecorationChrome &chrome, bool maximized)
{
    const qreal title = decorationTitleHeight(chrome);
    return maximized ? QMarginsF(0.0, title, 0.0, 0.0) : QMarginsF(1.0, title, 1.0, 1.0);
}

QSizeF decorationButtonCell(const DecorationChrome &chrome)
{
    const auto &style = decorationButtonStyle(chrome.buttonStyle);
    const qreal title = decorationTitleHeight(chrome);
    if (style.size <= 0.0) {
        // Full-height cells: the size option widens them instead.
        return {static_cast<qreal>(qRound(title * style.aspect * chrome.buttonScale)), title};
    }
    const qreal height = std::min(static_cast<qreal>(qRound(style.size * chrome.buttonScale)),
                                  title);
    return {height * style.aspect, height};
}

qreal decorationButtonGap(const DecorationChrome &chrome)
{
    const auto &style = decorationButtonStyle(chrome.buttonStyle);
    // A joined capsule has no gaps to scale.
    if (style.shape == DecorationButtonShape::Pill) {
        return 0.0;
    }
    return static_cast<qreal>(qRound(style.spacing * chrome.spacingScale));
}

} // namespace QindaQt::Decoration
