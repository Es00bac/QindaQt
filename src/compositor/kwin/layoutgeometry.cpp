// SPDX-License-Identifier: GPL-3.0-or-later
#include "layoutgeometry.h"

#include <cmath>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

void collect(const Core::LayoutNode &node, QSet<QString> &ids)
{
    if (node.isLeaf()) {
        ids.insert(node.windowId());
        return;
    }
    collect(*node.firstChild(), ids);
    collect(*node.secondChild(), ids);
}

void assignFrames(const Core::LayoutNode &node,
                  const QRectF &frame,
                  QHash<QString, QRectF> &frames)
{
    if (node.isLeaf()) {
        frames.insert(node.windowId(), frame);
        return;
    }

    const auto ratio = node.ratio().value_or(0.5);
    QRectF first = frame;
    QRectF second = frame;
    if (node.orientation() == Core::SplitOrientation::Horizontal) {
        const qreal firstWidth = std::floor(frame.width() * ratio);
        first.setWidth(firstWidth);
        second.setLeft(first.right());
    } else {
        const qreal firstHeight = std::floor(frame.height() * ratio);
        first.setHeight(firstHeight);
        second.setTop(first.bottom());
    }
    assignFrames(*node.firstChild(), first, frames);
    assignFrames(*node.secondChild(), second, frames);
}

} // namespace

LayoutGeometry LayoutGeometryPlanner::plan(const Core::WindowContainer &container,
                                           const QRectF &outerFrame)
{
    LayoutGeometry result;
    for (const auto &page : container.pages()) {
        collect(page.root(), result.allWindows);
        assignFrames(page.root(), outerFrame, result.frames);
        if (page.id() == container.activePageId()) {
            collect(page.root(), result.visibleWindows);
        }
    }
    return result;
}

QSet<QString> LayoutGeometryPlanner::windowIds(const Core::WindowContainer &container)
{
    QSet<QString> result;
    for (const auto &page : container.pages()) {
        collect(page.root(), result);
    }
    return result;
}

QSet<QString> LayoutGeometryPlanner::activeWindowIds(
    const Core::WindowContainer &container)
{
    QSet<QString> result;
    if (const auto *page = container.page(container.activePageId())) {
        collect(page->root(), result);
    }
    return result;
}

} // namespace QindaQt::Compositor::KWinIntegration
