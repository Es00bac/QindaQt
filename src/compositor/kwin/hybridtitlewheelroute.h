// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::Compositor::KWinIntegration {

// What a wheel turned away from the user over a *native* window title bar
// means, once the shared-chrome router has declined it (ADR-0203, ADR-0282).
enum class TitleWheelRoute {
    None,
    IconifyWindow,
    RollUpContainer,
};

// Facts the KWin adapter reads for the topmost input-eligible window under
// the pointer. Value-only so the routing rule is testable without KWin.
struct TitleWheelFacts final
{
    bool onTitleBar = false;
    bool normalWindow = false;
    bool iconified = false;
    bool grouped = false;
    // Whether the owning container's shared chrome is published and visible.
    // It is hidden while one member presents alone (member maximize focus).
    bool containerChromeVisible = false;
};

// AGENT-CONTRACT: an independent window rolls up to its icon chip. A grouped
// member's handlebar wheel belongs to the chrome router while its container
// chrome is visible, so nothing happens here; while that chrome is hidden
// (a member zoomed to fill the group) the member's own title is the group's
// only title, and a roll-up there rolls up the whole container. Before
// ADR-0282 that case did nothing, which left a zoomed group with no way to
// roll up from the pointer.
[[nodiscard]] constexpr TitleWheelRoute titleWheelRoute(const TitleWheelFacts &facts) noexcept
{
    if (!facts.onTitleBar || !facts.normalWindow || facts.iconified) {
        return TitleWheelRoute::None;
    }
    if (!facts.grouped) {
        return TitleWheelRoute::IconifyWindow;
    }
    return facts.containerChromeVisible ? TitleWheelRoute::None
                                        : TitleWheelRoute::RollUpContainer;
}

} // namespace QindaQt::Compositor::KWinIntegration
