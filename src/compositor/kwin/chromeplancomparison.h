// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QFlags>

namespace QindaQt::Compositor::KWinIntegration {

// How two chrome render plans for the same container differ, classified by
// what a repaint would have to touch. Pure value comparison; no rendering.
enum class ChromePlanDifference : quint8 {
    // Text the plan carries but the renderer never paints: member titles,
    // which belong to each member's own KDecoration handlebar.
    UnpaintedText = 0x1,
    // Painted title-row text: tab titles and the rolled-up badge label.
    TitleText = 0x2,
    // Tab rectangles or split-deck depths, e.g. one frame of a deck glide.
    TabGeometry = 0x4,
    // Anything else: frames, state, identity, style, buttons, members,
    // dividers, or a changed tab or member set.
    Structure = 0x8,
};
Q_DECLARE_FLAGS(ChromePlanDifferences, ChromePlanDifference)

// AGENT-GUARD: this compares every ChromeRenderPlan field except
// ChromeStyle::buttonPainter, a std::function that cannot be compared. A style
// change that swaps only the painter must reach overlays through
// KWinChromeManager::invalidateRendering(); the session does that in
// setChromeStyle()/setChromePalette(). A new plan field must be added here, or
// a change to it alone would be classified as no change and never painted;
// the static_asserts in the implementation fail the build when the struct
// layout changes so that this cannot be forgotten.
[[nodiscard]] ChromePlanDifferences compareChromePlans(
    const HybridChrome::ChromeRenderPlan &previous,
    const HybridChrome::ChromeRenderPlan &next);

} // namespace QindaQt::Compositor::KWinIntegration

Q_DECLARE_OPERATORS_FOR_FLAGS(
    QindaQt::Compositor::KWinIntegration::ChromePlanDifferences)
