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
    HybridDiagnostics unrolledDiagnostics;
};

// Drives the production group context menu's "Roll up group"/"Unroll group"
// entry (the fifth action, index 4: Arrange windows, Detach active window,
// Ungroup, Minimize group, Roll up/Unroll group) through real synthetic
// input against a live, already-grouped container. Proves the exact claims
// ADR-0099 depends on: shading never changes either member's real KWin
// frame, and each member is genuinely Window::isHidden() (not just
// compositor bookkeeping) while shaded and restored afterward.
//
// AGENT-NOTE: does NOT yet prove "moving the rolled strip must affect the
// restored position correctly" — dragging the shaded strip to a new
// position and unrolling there was attempted and is not yet working; see
// ops/team/messages for the live-run findings (confirmed via a real nested
// run and a new shadedStripFrames diagnostic: pointer.drag() at a point
// geometrically inside ChromeLayoutEngine's real outerTitleDragRect for the
// shaded (tabs-empty) plan does not move the strip, so the gap looks like
// input-intent routing rather than test geometry). Left as a known,
// reported gap rather than a permanently-failing assertion.
[[nodiscard]] std::optional<HybridPointerShadeEvidence>
exerciseHybridPointerShade(CompositorProbeClient &client,
                          HybridPointerGrouping &pointer,
                          const HybridPointerGroupedState &state,
                          const WindowInventory &grouped,
                          const QPointF &sharedTitlePoint,
                          QString *error);

} // namespace QindaQt::Test
