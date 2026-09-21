# ADR-0232: The gather overview replaces KWin's upper-left corner

- **Status:** Proposed
- **Date:** 2026-09-21
- **Owners:** Shell presentation
- **Supersedes:** None
- **Superseded by:** None

## Context

The operator asked for a specific arrangement, not a generic expose: iconified
window icons in a lane down the left, rolled-up container cards in a column
immediately right of those, and every remaining window in a wheel-scrollable
grid filling the rest, all inset by a buffer and anchored upper-left. It is a
temporary view — nothing moves, a click raises one window, Escape or a click
that misses everything dismisses it — and it should answer the same gesture
KWin's own overview effect answers: brushing the upper-left screen corner.

KWin already reserves that corner. `Effect-overview`'s `BorderActivate`
defaults to `ElectricTopLeft`, so on a stock machine the corner raises KWin's
window grid. That grid cannot express the arrangement above: it has no notion
of QindaQt's iconified windows (ADR-0203) or of hybrid containers, so it shows
a container's members as unrelated windows and an iconified window not at all.

Two layers already hold most of what such a view needs. `hybrid_gather` plans
rectangles, and the task-list applet's controller already publishes the
authoritative window list with its grants, its icon-name policy (ADR-0230),
and stale-revision arbitration on every intent.

## Decision

**QindaQt claims the corner, and the corner, a global shortcut and a panel
button all reach one controller.**

`SessionDefaults` seeds `Effect-overview/BorderActivate` to an empty list, so
KWin reserves no corner. Seed-missing only: an operator who reassigned that
corner keeps their choice. KWin's effect keeps its own keyboard shortcut, so
nothing is taken away, only un-reserved.

`GatherOverviewController` holds the overview's entire mutable state — open or
not, and how far the grid is scrolled — as Qt Core plus moc, so the policy
half qualifies without a display. Four decisions live there and nowhere else:
a closed overview projects nothing; opening starts at the top; `scrollBy` adds
a delta and re-projects rather than clamping, because the planner is the only
thing allowed to clamp; and an activation is refused unless the item is in the
current projection *and* that projection is interactive.

`GatherOverviewComposition` owns that controller, borrows the task-list applet
controller, and turns `activationRequested` back into that controller's task
intents. The overview therefore has **no authority of its own**: it inherits
the applet's grants and its arbitration. A session whose `windows.read` grant
was denied gets a composition that never opens — there is nothing truthful to
show and nothing it would be permitted to do.

Three doors, one controller: `Meta+G`
(`qindaqt_toggle_gather_overview`), the `gather-overview` panel applet, and
the compositor's `overview` edge gesture (ADR-0205), which the corner and a
touch swipe both announce. Because all three call `toggle()` on the same
object, they cannot disagree about whether the overview is up.

## Consequences

The surface is a per-output layer-shell window on `LayerOverlay` with a zero
exclusive zone, created once per output and then shown and hidden. Zero and
not −1: the overview covers the panel while up but must never reserve space,
or every window in the session would be resized the first time it opened.
Creating it once costs a first-frame wait once rather than on every flick.

`closeOnDismissed` is deliberately **false**. It would make LayerShellQt close
the window directly, leaving the controller still believing the overview is
up — and since the controller is what all three doors read, the next `Meta+G`
would close an overview nobody can see. Dismissal reaches the controller
through the surface's own `dismissRequested` instead.

The scope is `gather-overview`, not `desktop`. KWin maps exactly the `desktop`
scope to `WindowType::Desktop`, which the wallpaper and the desktop-icon
surface need and this one must not have: it sits above windows, not behind
them.

Window tiles draw identity, not thumbnails. The authenticated preview channel
(ADR-0119) is the only sanctioned way to get window pixels, and the tile
leaves a documented seam for it rather than inventing a second path.
