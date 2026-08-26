// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_input/interactiontypes.h"

namespace QindaQt::Compositor::KWinIntegration {

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
