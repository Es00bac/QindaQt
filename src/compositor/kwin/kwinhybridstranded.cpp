// SPDX-License-Identifier: GPL-3.0-or-later
//
// Output-removal rescue for KWinHybridSession (ADR-0384). KWin evacuates
// ordinary windows when an output goes away; this pass catches what Hybrid
// itself would otherwise put back there: saved container frames, and any
// independent window left with a frame that overlaps no remaining output.
//
// AGENT-NOTE: a translation unit of KWinHybridSession like the other
// kwinhybrid*.cpp files; it owns no state. Container layouts are rescued by
// HybridContainerPlacementController::refreshStrandedContainers() and
// refreshMaximizedAreas(), and independent restore states by the scene
// platform when it applies them; this is the last check after all of them.

#include "kwinhybridsession.h"

#include "hybridcontainerplacement.h"
#include "kwinrescueoutputs.h"
#include "managedwindowregistry.h"

#include <window.h>

namespace QindaQt::Compositor::KWinIntegration {

void KWinHybridSession::rescueStrandedWindows()
{
    if (m_shutdown || !ready()) {
        return;
    }
    const auto outputs = currentRescueOutputs();
    if (outputs.isEmpty()) {
        return;
    }
    if (m_placement) {
        const auto moved = m_placement->refreshStrandedSavedFrames(outputs);
        if (!moved.isEmpty()) {
            qWarning("QindaQt Hybrid moved saved frames off a removed output for: %s",
                     qPrintable(moved.join(QStringLiteral(", "))));
        }
    }
    for (const auto &windowId : m_registry.windowIds()) {
        // A grouped member follows its container's layout, which the
        // placement controller has just rescued; an iconified window is a
        // hidden client whose chip is clamped to its output on publication.
        if (!m_registry.owner(windowId).isEmpty() || isWindowIconified(windowId)) {
            continue;
        }
        auto *const window = m_registry.window(windowId);
        if (!window || window->isDeleted() || !window->isPlaceable()) {
            continue;
        }
        // AGENT-GUARD: KWin owns the geometry of fullscreen, maximized and
        // quick-tiled windows and re-fits them to their output itself; a
        // plain moveResize() here would fight that special-state geometry.
        if (window->isRequestedFullScreen()
            || window->requestedMaximizeMode() != KWin::MaximizeRestore
            || window->requestedQuickTileMode()
                != KWin::QuickTileMode(KWin::QuickTileFlag::None)) {
            continue;
        }
        const QRectF frame = window->moveResizeGeometry();
        const auto rescued = rescuedFrame(frame, outputs);
        if (!rescued) {
            continue;
        }
        qWarning("QindaQt brought back window '%s' left at %dx%d%+d%+d, outside every output",
                 qPrintable(windowId), qRound(frame.width()), qRound(frame.height()),
                 qRound(frame.x()), qRound(frame.y()));
        window->moveResize(*rescued);
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
