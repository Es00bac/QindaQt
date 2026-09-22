// SPDX-License-Identifier: GPL-3.0-or-later
//
// Output-removal rescue for HybridContainerPlacementController: bringing back
// a container whose frame no longer overlaps any work area.
//
// AGENT-NOTE: split out of hybridcontainerplacement.cpp, which was at its
// 600-line shape limit, following the same split as hybridcontainershade.cpp.
// This is a cohesive slice -- nothing here touches the drag, resize, maximize,
// shade or aspect-pin state machines -- so the split is by behaviour rather
// than by line count. This is the same class, a second translation unit.

#include "hybridcontainerplacement.h"

#include <QStringList>

#include <algorithm>
#include <optional>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

namespace {

// The nearest frame of the same size that lies inside `area`, shrunk only when
// it cannot fit. QRect::right()/bottom() are inclusive, hence the +1.
[[nodiscard]] QRect containedWithin(const QRect &frame, const QRect &area)
{
    QRect result = frame;
    if (result.width() > area.width()) {
        result.setWidth(area.width());
    }
    if (result.height() > area.height()) {
        result.setHeight(area.height());
    }
    result.moveTo(
        std::clamp(result.left(), area.left(), area.right() - result.width() + 1),
        std::clamp(result.top(), area.top(), area.bottom() - result.height() + 1));
    return result;
}

} // namespace

QStringList HybridContainerPlacementController::refreshStrandedContainers()
{
    QStringList failures;
    if (!m_topology) {
        return failures;
    }
    QStringList containerIds = m_topology().containerIds();
    containerIds.sort();
    for (const auto &containerId : std::as_const(containerIds)) {
        // Maximized containers belong to refreshMaximizedAreas(), which knows
        // their restore frame; shaded ones track an independent strip frame.
        if (isMaximized(containerId) || isShaded(containerId)) {
            continue;
        }
        const auto current = m_layout ? m_layout(containerId) : std::nullopt;
        const auto workArea = m_workArea ? m_workArea(containerId) : QRect{};
        if (!current || !current->outerFrame.isValid() || !workArea.isValid()) {
            continue;
        }
        // Only a container with no overlap at all is stranded. One hanging off
        // an edge is still reachable and is the user's own arrangement.
        if (current->outerFrame.intersects(workArea)) {
            continue;
        }
        const QRect target = containedWithin(current->outerFrame, workArea);
        if (target == current->outerFrame) {
            continue;
        }
        QString error;
        if (!reflow(containerId, target, &error)) {
            failures.append(QStringLiteral("container '%1': %2")
                                .arg(containerId, error));
        }
    }
    return failures;
}

} // namespace QindaQt::Compositor::KWinIntegration
