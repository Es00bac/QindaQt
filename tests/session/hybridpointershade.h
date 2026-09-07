// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compositorprobeclient.h"
#include "hybridpointerinventory.h"

#include <QPointF>

#include <functional>
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

struct HybridPointerShadeOcclusionEvidence final
{
    WindowInventory occluded;
    WindowInventory raised;
    WindowInventory unrolled;
};

// Proves the repair for root's recorded bounded limitation: an unrelated
// window partially covering a shaded strip, then a press on the strip's
// still-exposed sliver, must raise the strip's real KWin stack position back
// above the occluder (RaiseActivation::RaiseOnly's z-order raise) without
// granting native activation to the hidden anchor (see
// KWinHybridGroupStacking::RaiseActivation and
// dispatchChromePointerDecision/executeShellWindowAction's identical guard)
// -- so both real members stay exactly as hidden and frozen as they were
// before the click, never activated or unhidden as a side effect. Ends by
// unrolling at the (undisturbed) original point and checking geometry
// restores exactly, proving the raise did not leave the shaded/placement
// bookkeeping in a state unroll cannot recover from.
[[nodiscard]] std::optional<HybridPointerShadeOcclusionEvidence>
exerciseHybridPointerShadeOcclusionRaise(
    CompositorProbeClient &client,
    HybridPointerGrouping &pointer,
    const HybridPointerGroupedState &state,
    const WindowInventory &grouped,
    const QPointF &sharedTitlePoint,
    const std::function<void(const QString &)> &activateProbe,
    QString *error);

} // namespace QindaQt::Test
