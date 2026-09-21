// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

namespace QindaQt::HybridGather {

// Pure geometry for the gather overview: one deterministic arrangement of
// everything the session currently holds, in three lanes across the output.
//
// The overview is transient. Nothing here moves a real window: the caller
// draws tiles at these rectangles, activates whatever the user clicks, and
// dismisses. That is why a tile carries a source size but no window state.
//
// Left to right:
//
//   +--------------------------------------------------------------+
//   |            (margin all round, 90 px by default)              |
//   |   O    +----------+    +-------+ +-------+ +-------+         |
//   |        |   card   |    | win   | | win   | | win   |         |
//   |   O    +----------+    +-------+ +-------+ +-------+         |
//   |        +----------+    +-------+ +-------+                   |
//   |   O    |   card   |    | win   | | win   |    scrolls        |
//   |        +----------+    +-------+ +-------+                   |
//   +--------------------------------------------------------------+
//    icons     cards            window grid
//
// - icons: one round chip per iconified window (ADR-0203), in iconify order;
// - cards: one strip per rolled-up container (ADR-0139). Gather rolls every
//   container up, so this lane holds them all, not only the ones already
//   rolled up;
// - windows: everything else, in a grid that scrolls vertically. A container
//   member is never here — its container is a card.
//
// AGENT-CONTRACT: this header is the whole contract. The planner takes no
// KWin type, reads no global state, and returns rectangles in the same
// desktop-logical frame as `workArea`, so the caller's only job is to draw
// and hit-test them. Ordering of every output vector matches its input.
struct GatherTile final
{
    QString id;
    // The tile's natural size, used to fit it inside its grid cell without
    // distorting it. A non-positive size falls back to filling the cell.
    QSizeF sourceSize;

    friend bool operator==(const GatherTile &, const GatherTile &) = default;
};

struct GatherRequest final
{
    // The output's work area in desktop-logical coordinates.
    QRectF workArea;
    // Buffer between the arrangement and the work area's edges.
    qreal margin = 90;
    // Space between lanes and between items within a lane.
    qreal gap = 16;
    // One iconified window's round chip is square with this extent.
    qreal iconExtent = 48;
    // One rolled-up container's card.
    QSizeF cardSize = QSizeF(240, 64);
    // One window's grid cell.
    QSizeF cellSize = QSizeF(260, 176);

    // Iconify order, oldest first.
    QVector<QString> iconifiedWindowIds;
    // Container order.
    QVector<QString> containerIds;
    // Free windows: not iconified, and not a member of any container.
    QVector<GatherTile> windows;

    // Wheel position in the window grid, clamped into range by the planner.
    qreal scrollOffset = 0;
};

struct GatherPlacement final
{
    QString id;
    QRectF frame;
    // False when the item lies outside its lane or the scrolled viewport. The
    // caller must not draw or hit-test an invisible placement.
    bool visible = true;

    friend bool operator==(const GatherPlacement &, const GatherPlacement &) = default;
};

struct GatherLayout final
{
    // False only for an unusable request: a non-finite or empty work area, a
    // negative margin or gap, a non-positive extent, or a margin that leaves
    // no field at all. `diagnostic` then says which.
    bool ok = false;
    QString diagnostic;

    // The work area inset by the margin: everything below sits inside it.
    QRectF field;
    QVector<GatherPlacement> icons;
    QVector<GatherPlacement> cards;
    QVector<GatherPlacement> windows;

    // The window grid's clipping rectangle. Empty when the icon and card
    // lanes leave less than one cell of width, in which case `gridColumns` is
    // zero and every window placement is invisible — the honest outcome for a
    // session with hundreds of iconified windows, rather than a mangled grid.
    QRectF gridViewport;
    int gridColumns = 0;
    int gridRows = 0;
    qreal gridContentHeight = 0;
    qreal maximumScrollOffset = 0;
    // `scrollOffset` clamped to [0, maximumScrollOffset].
    qreal appliedScrollOffset = 0;

    friend bool operator==(const GatherLayout &, const GatherLayout &) = default;
};

// Deterministic: the same request always yields the same layout.
[[nodiscard]] GatherLayout planGather(const GatherRequest &request);

} // namespace QindaQt::HybridGather
