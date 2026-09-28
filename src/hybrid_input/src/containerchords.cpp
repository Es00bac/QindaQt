// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_input/containerchords.h"

namespace QindaQt::HybridInput {

Qt::KeyboardModifiers windowManagementModifier(
    const std::optional<Qt::KeyboardModifiers> &dockChord) noexcept
{
    if (!dockChord) {
        return Qt::NoModifier;
    }
    return *dockChord & ~Qt::KeyboardModifiers(Qt::ShiftModifier);
}

PointerChordAction classifyPointerChord(const PointerChordFacts &facts) noexcept
{
    if (!facts.dockChord) {
        return PointerChordAction::None;
    }
    if (facts.button == Qt::LeftButton && facts.modifiers == *facts.dockChord) {
        return PointerChordAction::MemberDock;
    }
    const auto modifier = windowManagementModifier(facts.dockChord);
    if (modifier == Qt::NoModifier || facts.modifiers != modifier) {
        return PointerChordAction::None;
    }
    if (facts.button == Qt::LeftButton) {
        return facts.overContainer ? PointerChordAction::ContainerMove
                                   : PointerChordAction::None;
    }
    if (facts.button == Qt::RightButton) {
        if (facts.overContainer) {
            return PointerChordAction::ContainerResize;
        }
        return facts.overIndependentWindow ? PointerChordAction::WindowResize
                                           : PointerChordAction::None;
    }
    return PointerChordAction::None;
}

Qt::Edges nearestResizeEdges(const QRectF &frame, const QPointF &position) noexcept
{
    if (!frame.isValid() || !frame.contains(position)) {
        return Qt::RightEdge | Qt::BottomEdge;
    }
    const qreal x = position.x() - frame.x();
    const qreal y = position.y() - frame.y();
    const bool left = x < frame.width() / 3.0;
    const bool right = x >= 2.0 * frame.width() / 3.0;
    const bool top = y < frame.height() / 3.0;
    const bool bottom = y >= 2.0 * frame.height() / 3.0;
    Qt::Edges edges;
    if (top) {
        edges |= Qt::TopEdge;
    } else if (bottom) {
        edges |= Qt::BottomEdge;
    }
    if (left) {
        edges |= Qt::LeftEdge;
    } else if (right) {
        edges |= Qt::RightEdge;
    }
    if (!top && !bottom && !left && !right) {
        // The middle cell: the nearer vertical side, as KWin does.
        edges = x < frame.width() / 2.0 ? Qt::LeftEdge : Qt::RightEdge;
    }
    return edges;
}

} // namespace QindaQt::HybridInput
