// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/compositor_capture/region_geometry.h>

#include <QtMath>

#include <algorithm>

namespace QindaQt::CompositorCapture {

QRect WorkspaceFrame::logicalBounds() const
{
    if (image.isNull() || scale <= 0)
        return {};
    return {origin, QSize(qCeil(image.width() / scale), qCeil(image.height() / scale))};
}

QRect selectionFromPoints(QPoint anchor, QPoint current, const QRect &bounds)
{
    // AGENT-GUARD: not QRect(p1, p2).normalized(): Qt 6 shifts swapped edges
    // by one pixel, so an up-left drag would lose a row and a column. A drag
    // from x to x+10 selects the 10 pixels the pointer swept, either way.
    const int left = std::min(anchor.x(), current.x());
    const int top = std::min(anchor.y(), current.y());
    const int right = std::max(anchor.x(), current.x());
    const int bottom = std::max(anchor.y(), current.y());
    return QRect(left, top, right - left, bottom - top).intersected(bounds);
}

QRect nudgedSelection(const QRect &selection, int dx, int dy, bool resize, const QRect &bounds)
{
    if (selection.isEmpty() || bounds.isEmpty())
        return selection;
    if (resize) {
        // AGENT-GUARD: std::clamp is undefined when hi < lo; a selection
        // starting outside `bounds` must still get a valid upper limit.
        const int maxWidth = std::max(1, bounds.right() - selection.left() + 1);
        const int maxHeight = std::max(1, bounds.bottom() - selection.top() + 1);
        const int width = std::clamp(selection.width() + dx, 1, maxWidth);
        const int height = std::clamp(selection.height() + dy, 1, maxHeight);
        return {selection.topLeft(), QSize(width, height)};
    }
    const int width = std::min(selection.width(), bounds.width());
    const int height = std::min(selection.height(), bounds.height());
    const int left = std::clamp(selection.left() + dx, bounds.left(), bounds.right() - width + 1);
    const int top = std::clamp(selection.top() + dy, bounds.top(), bounds.bottom() - height + 1);
    return {left, top, width, height};
}

QRect keyboardStartSelection(const QRect &screen)
{
    if (screen.isEmpty())
        return {};
    const QSize size(std::max(1, screen.width() / 2), std::max(1, screen.height() / 2));
    return {screen.left() + (screen.width() - size.width()) / 2,
            screen.top() + (screen.height() - size.height()) / 2, size.width(), size.height()};
}

QRect toImagePixels(const WorkspaceFrame &frame, const QRect &logical)
{
    if (frame.image.isNull() || logical.isEmpty() || frame.scale <= 0)
        return {};
    const qreal left = (logical.left() - frame.origin.x()) * frame.scale;
    const qreal top = (logical.top() - frame.origin.y()) * frame.scale;
    const qreal right = (logical.left() + logical.width() - frame.origin.x()) * frame.scale;
    const qreal bottom = (logical.top() + logical.height() - frame.origin.y()) * frame.scale;
    const QRect pixels(QPoint(qFloor(left), qFloor(top)), QPoint(qCeil(right) - 1, qCeil(bottom) - 1));
    return pixels.intersected(frame.image.rect());
}

QImage cropRegion(const WorkspaceFrame &frame, const QRect &logical)
{
    const QRect pixels = toImagePixels(frame, logical);
    return pixels.isEmpty() ? QImage() : frame.image.copy(pixels);
}

} // namespace QindaQt::CompositorCapture
