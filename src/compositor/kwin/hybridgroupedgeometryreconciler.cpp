// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridgroupedgeometryreconciler.h"

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

HybridGroupedGeometryReconciler::HybridGroupedGeometryReconciler(
    GroupedGeometrySnapshot snapshot,
    GroupedGeometryApply apply)
    : m_snapshot(std::move(snapshot))
    , m_apply(std::move(apply))
{
}

QStringList HybridGroupedGeometryReconciler::reconcile() const
{
    if (!m_snapshot || !m_apply) {
        return {QStringLiteral("grouped geometry dependencies are unavailable")};
    }

    auto windows = m_snapshot();
    std::ranges::sort(windows, {}, &GroupedWindowGeometry::windowId);
    QStringList failures;
    for (const auto &window : windows) {
        if (window.containerId.isEmpty() || window.nativeFrameOverride) {
            continue;
        }
        if (window.windowId.isEmpty() || !window.targetFrame.isValid()) {
            failures.append(QStringLiteral("owned window '%1' has no valid target frame")
                                .arg(window.windowId));
            continue;
        }
        // AGENT-GUARD: Compare KWin's requested geometry, not frameGeometry().
        // Wayland clients acknowledge configure requests asynchronously; using
        // the last painted frame here would create a configure feedback loop.
        if (window.requestedFrame == window.targetFrame) {
            continue;
        }
        QString error;
        if (!m_apply(window.windowId, window.targetFrame, &error)) {
            failures.append(QStringLiteral("window '%1': %2")
                                .arg(window.windowId,
                                     error.isEmpty()
                                         ? QStringLiteral("native geometry was rejected")
                                         : error));
        }
    }
    return failures;
}

} // namespace QindaQt::Compositor::KWinIntegration
