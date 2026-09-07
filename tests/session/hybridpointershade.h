// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compositorprobeclient.h"
#include "hybridpointerinventory.h"

#include <QPointF>

#include <optional>

namespace QindaQt::Test {

class HybridPointerGrouping;
struct HybridPointerGroupedState;

struct HybridPointerShadeEvidence final
{
    WindowInventory shaded;
    WindowInventory unrolled;
    HybridDiagnostics shadedDiagnostics;
    HybridDiagnostics movedDiagnostics;
    HybridDiagnostics unrolledDiagnostics;
    QPointF dragDelta;
};

// Drives the production group context menu's "Roll up group"/"Unroll group"
// entry (the fifth action, index 4: Arrange windows, Detach active window,
// Ungroup, Minimize group, Roll up/Unroll group) through real synthetic
// input against a live, already-grouped container. Proves the exact claims
// ADR-0099 depends on: shading never changes either member's real KWin
// frame, and each member is genuinely Window::isHidden() (not just
// compositor bookkeeping) while shaded and restored afterward.
//
// Also drags the rolled strip to a new position and unrolls it there,
// proving HybridContainerPlacementController::handleShadedMove and the
// group-stacking fix that keeps the shaded strip's chrome overlay published
// (see KWinHybridGroupStacking::synchronize's shadedAnchors parameter):
// dragging previously never reached the strip at all because
// chromeOverlayCount/publishedGroupStackingCount dropped to 0 immediately
// after the first shade (see ops/team/messages for that live-run finding).
// The drag translates the strip by an exact, known delta; unroll performs
// its one real reflow to the original size at that dragged position, so
// both members' post-unroll frames must equal their pre-shade frames
// translated by the same delta (divider ratios and container size are
// unchanged by a pure-translation reflow; only the origin moves).
[[nodiscard]] std::optional<HybridPointerShadeEvidence>
exerciseHybridPointerShade(CompositorProbeClient &client,
                          HybridPointerGrouping &pointer,
                          const HybridPointerGroupedState &state,
                          const WindowInventory &grouped,
                          const QPointF &sharedTitlePoint,
                          QString *error);

} // namespace QindaQt::Test
