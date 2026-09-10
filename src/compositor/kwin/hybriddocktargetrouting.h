// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_input/interactiontypes.h"

#include <QPointF>
#include <QRectF>

namespace QindaQt::Compositor::KWinIntegration {

// Container-level drop discovery. A pointer within this band of a container's
// content frame targets the whole container (its active page root, expressed
// as a DockTarget with an empty memberId) instead of the member tile under the
// pointer, so an edge drop adds a full-height or full-width tile beside the
// existing layout. Returns None outside the band or for an invalid frame;
// callers then fall back to the member-tile zones.
[[nodiscard]] HybridInput::DockZone containerEdgeDockZone(
    const QRectF &contentFrame, const QPointF &position);
// Band thickness in logical pixels for one content frame: a fraction of the
// shorter side, clamped so large containers keep a reachable member zone and
// small ones keep a reachable interior.
[[nodiscard]] qreal containerEdgeBand(const QRectF &contentFrame);

// Converts a shared-chrome tab hit into the semantic docking target consumed
// by HybridInteractionRuntime. Other chrome regions deliberately fall through
// to KWin window geometry routing.
[[nodiscard]] HybridInput::DockTarget tabDockTargetFromChromeHit(
    const HybridInput::HitTarget &hit);

// Resolves exact-modifier presses from the same stack-relative exposure result
// used by ordinary chrome input. A same-container member still owns its native
// frame: scene dividers and tabs must not create holes through client content.
[[nodiscard]] HybridInput::HitTarget sourceHitRespectingChromeExposure(
    bool chromeExposed,
    bool nativeIsChromeMember,
    const HybridInput::HitTarget &nativeTitle,
    const HybridInput::HitTarget &chromeHit);

// Applies the equivalent rule to pointer drop discovery. nativeTarget may be
// invalid when the topmost input owner is a dialog, popup, or internal window;
// that invalid value is an intentional barrier against searching underneath.
[[nodiscard]] HybridInput::DockTarget dockTargetRespectingChromeExposure(
    bool chromeExposed,
    bool nativeIsChromeMember,
    const HybridInput::DockTarget &chromeTarget,
    const HybridInput::DockTarget &nativeTarget);

} // namespace QindaQt::Compositor::KWinIntegration
