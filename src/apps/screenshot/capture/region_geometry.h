// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>

namespace QindaQt::Screenshot {

// The frozen workspace a region is cut from: KWin's CaptureWorkspace image,
// the logical position of its top-left pixel, and image pixels per logical
// pixel.
struct WorkspaceFrame {
    QImage image;
    QPoint origin;
    qreal scale = 1.0;

    // The logical rectangle the image covers.
    [[nodiscard]] QRect logicalBounds() const;
};

// Selection geometry for the region overlay. All rectangles are logical,
// global desktop coordinates, so a drag may cross outputs.
//
// AGENT-CONTRACT: the overlay QML calls these through the session object;
// selection policy lives here, where rows can pin it, never in QML math.
[[nodiscard]] QRect selectionFromPoints(QPoint anchor, QPoint current, const QRect &bounds);

// Arrow-key editing. Without `resize` the selection moves by (dx, dy) and
// stays whole inside `bounds`; with `resize` its right/bottom edge moves and
// the selection keeps at least one logical pixel.
[[nodiscard]] QRect nudgedSelection(const QRect &selection, int dx, int dy, bool resize,
                                    const QRect &bounds);

// The starting selection when the user edits with the keyboard before
// dragging: the middle half of the output they are on.
[[nodiscard]] QRect keyboardStartSelection(const QRect &screen);

// Image pixels for a logical selection, clamped to the image. Edges are
// rounded outward so a fractional scale never drops a selected pixel row.
[[nodiscard]] QRect toImagePixels(const WorkspaceFrame &frame, const QRect &logical);
[[nodiscard]] QImage cropRegion(const WorkspaceFrame &frame, const QRect &logical);

} // namespace QindaQt::Screenshot
