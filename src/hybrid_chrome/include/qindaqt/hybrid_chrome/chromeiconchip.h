// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "chromeidentity.h"
#include "chrometypes.h"

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <optional>

class QPainter;

namespace QindaQt::HybridChrome {

// The iconified-window chip (ADR-0203): an ordinary window rolled up to its
// application icon. Layout is a pure value transformation of one request;
// paint reads only the plan; hit testing is a pure geometry query. Nothing
// here owns a window, a scene item, or input.
enum class IconChipHitKind {
    None,
    Body,
    Close,
};

struct IconChipRequest final
{
    QString windowId;
    // Hover label (the window caption). Empty paints no label.
    QString title;
    // Application icon at any size; a null image paints the placeholder glyph.
    QImage icon;
    // Global logical top-left the chip anchors to: the window's title-bar
    // leading edge at iconify time, or the chip's dragged position later.
    QPointF anchor;
    // Global logical rectangle the chip is kept inside (the output). An
    // invalid rectangle disables clamping.
    QRectF bounds;
    qreal devicePixelRatio = 1.0;
    // Theme scale multiplier for the 48 px pill; clamped to [0.5, 3].
    qreal scale = 1.0;
    ChromePalette palette;
    // Invalid derives every identity shade from the palette accent.
    QColor identityColor;
    bool focused = false;
};

struct IconChipPlan final
{
    QString windowId;
    QString title;
    qreal devicePixelRatio = 1.0;
    qreal scale = 1.0;
    // The round pill itself; all rectangles share global logical coordinates.
    QRectF frame;
    QRectF iconRect;
    // Close glyph target, straddling the pill's top-right edge. It is painted
    // only while the chip is hovered; a hit here is only meaningful then.
    QRectF closeRect;
    // Hover label beside the pill; empty when the request carried no title.
    QRectF labelRect;
    // Union of every painted rectangle: the extent a scene image must cover.
    QRectF imageRect;
    ChromePalette palette;
    ChromeIdentityShades identity;
    QImage icon;
    bool focused = false;

    [[nodiscard]] bool isValid() const;
};

struct IconChipPaintState final
{
    bool hovered = false;
    bool closeHovered = false;
    bool pressed = false;
};

class ChromeIconChip final
{
public:
    static constexpr qreal ChipExtent = 48.0;
    static constexpr qreal IconExtent = 28.0;
    static constexpr qreal CloseExtent = 16.0;
    static constexpr qreal LabelGap = 6.0;
    static constexpr qreal LabelMaximumWidth = 240.0;
    static constexpr qreal MinimumScale = 0.5;
    static constexpr qreal MaximumScale = 3.0;

    // Rejects an empty window id, a non-finite anchor, ratio, or scale, and
    // an invalid palette. Clamps the pill into bounds when bounds is valid.
    [[nodiscard]] static std::optional<IconChipPlan> layout(
        const IconChipRequest &request, QString *error = nullptr);
    // Paints in the plan's global logical coordinates; callers translate.
    static void paint(QPainter &painter,
                      const IconChipPlan &plan,
                      const IconChipPaintState &state = {});
    // Close before body; the pill is hit-tested as the circle it paints.
    [[nodiscard]] static IconChipHitKind hitTest(const IconChipPlan &plan,
                                                 const QPointF &position);
};

} // namespace QindaQt::HybridChrome
