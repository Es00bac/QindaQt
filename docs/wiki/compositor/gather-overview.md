# Gather overview

Gather brings everything the session holds forward at once, in one
deterministic arrangement, and dismisses without moving anything. It replaces
KWin's own window grid on the top-left screen corner.

**Status: the planner, the presentation model, the controller and the drawing
surface are implemented and qualified; nothing invokes them yet.** What exists
today is `src/hybrid_gather` (the pure planner) and `src/shell/gather_overview`
(the model, the controller, and the `QindaQt.Shell.GatherOverview` QML
surface), plus the release of the hot corner. What is missing is session
plumbing only: a window to host the surface, a shortcut, and an applet button.
See [Remaining work](#remaining-work).

## The arrangement

Three lanes across the output's work area, inset by a buffer (90 logical
pixels by default):

```
+--------------------------------------------------------------+
|            (margin all round, 90 px by default)              |
|   O    +----------+    +-------+ +-------+ +-------+         |
|        |   card   |    | win   | | win   | | win   |         |
|   O    +----------+    +-------+ +-------+ +-------+         |
|        +----------+    +-------+ +-------+                   |
|   O    |   card   |    | win   | | win   |    scrolls        |
|        +----------+    +-------+ +-------+                   |
+--------------------------------------------------------------+
 icons     cards            window grid
```

- **Icons** — one round chip per iconified window
  ([ADR-0203](../adr/0203-an-ordinary-window-rolls-up-to-its-icon.md)), in iconify order,
  stacked down the field's left edge.
- **Cards** — one strip per container
  ([ADR-0139](../adr/0139-identity-borders-focus-and-rolled-up-badge.md)). Gather rolls **every**
  container up, so this lane holds all of them, not only those already rolled
  up.
- **Window grid** — everything else, row-major, in the remaining width, and it
  scrolls vertically with the wheel. A container member is never here: its
  container is a card.

Each lane stacks down the field and wraps into a new column to its right when a
column fills, which pushes the lanes to its right along.

## Contract

`planGather()` is pure: it takes a work area and three inventories and returns
rectangles in the same desktop-logical frame. It holds no KWin type, reads no
global state, and the same request always yields the same layout. Output
ordering matches input ordering, so a caller can zip results back onto its own
inventories by index.

Two rules exist because the alternative is a silently wrong picture:

- **A window is fitted inside its cell, never magnified.** A 3840x2160 window
  shrinks to fit; a 200x100 one keeps its size. Scaling a small window up to
  fill a cell only produces a blurry preview.
- **A viewport narrower than one cell yields zero columns and no visible
  window**, reported in `diagnostic`, rather than one column of clipped tiles
  overlapping the card lane. That is the honest outcome for a session with
  hundreds of iconified windows.

Every placement carries a `visible` flag. A caller must not draw or hit-test an
invisible placement: it is either scrolled out of the grid viewport or past the
end of its lane.

`ok` is false only for an unusable request — a non-finite or empty work area, a
negative margin or gap, a non-positive extent, or a margin that leaves no field
— and `diagnostic` then says which.

## The presentation model

`src/shell/gather_overview` answers the question the planner deliberately does
not: *which lane does each thing belong in?* `projectGatherOverview()` is pure
too. It takes the `TaskListAppletProjection` the shell already produces — the
same facts the task list and dock present — plus one work area, and returns
drawable items whose frames come straight from the planner.

The classification is three rules:

- a **container** row is a card, always;
- a **window** row that is iconified is an icon chip;
- every other window row is a grid tile.

Two consequences are worth stating because both read like something more than
they are:

- **"Gather rolls every container up" is a presentation rule, not an
  operation.** The overview is transient and moves nothing, so a container is
  *drawn* as a card whether or not it is really rolled up, and no roll-up
  intent is ever submitted.
- **A container member is never a grid tile** for free: in the grouped rows a
  container is one row, so its members are not rows at all.

A merely *minimized* window is not iconified ([ADR-0203](../adr/0203-an-ordinary-window-rolls-up-to-its-icon.md)
is specifically about rolling up to an icon), so it stays in the grid with the
rest.

The model also carries the truth a surface needs in order not to lie:

- `interactive` is false when the source phase is `Degraded` — the retained
  generation is still drawn, because it is the last thing the user actually
  saw, but every intent is fenced, so tiles must present as inert rather than
  let a click look like it worked.
- `available` is false for `Loading` and `Unavailable`, and for a work area the
  planner refuses; the counts then stay zero rather than describing a session
  that was never arranged.
- `windowsHidden` is how many grid tiles are not drawn — scrolled past, or in a
  viewport too narrow for one column — which is the only honest basis for a
  "there is more below" hint.
- `sourceOverflowCount` passes through what the task list's own presentation
  cap already dropped. The overview cannot show what it never received, and
  says so instead of implying completeness.

Every item carries `taskId`, the optional `windowId`, and the
`generationRevision` it was projected from, so an activation echoes the
generation and stale-revision arbitration can refuse an action against a
generation the user no longer sees.

Because there is no preview renderer yet, a grid tile is handed to the planner
with no source size, which fills its cell. When previews land, the preview's
own size goes in that field and the planner aspect-fits it — no change to this
policy.

## The hot corner

KWin's overview effect reserves the top-left screen corner by default: its
`BorderActivate` default is `ElectricTopLeft` (7), an `IntList`. QindaQt's
session defaults seed `[Effect-overview] BorderActivate=` empty, so no corner
is reserved and the effect keeps its own shortcut. The seed is **missing-only**:
a user who reassigned that corner keeps their choice
(`qindaqt.session-sessiondefaults` pins both halves).

## Remaining work

Live window previews are still unavailable: `WindowPreview` on
`CompositorShell1` is specified and the whole shell-side pipeline is
implemented, but the renderer behind it is not — the exported KWin 6.6 headers
offer no supported window-texture readback, so
[ADR-0119](../adr/0119-authenticated-window-preview-channel.md) records the
endpoint and renderer as the bounded remaining piece. It is the same gap as the
dock's hover thumbnails.

It no longer blocks gather, though. The card-and-icon variant — application icon plus title instead of a
thumbnail — is the route being taken, exactly because it needs none of that.
`src/shell/gather_overview` is that variant's model, and it is built so the
preview upgrade is additive: a tile's source size is the single field that
changes.

Remaining, in order:

1. A session window to host the surface — full-bleed over one output, above
   everything, taking keyboard focus so Escape reaches it — feeding it
   `controller.projection` and wiring the three signals back to
   `scrollBy`, `activate` and `close`.
2. Something to turn `activationRequested` into a task intent with the
   authority to do it. The controller deliberately stops one step short.
3. A `HybridShortcutAction` calling `toggle()`, beside the existing container
   shortcuts, so it has a keyboard trigger.
4. A panel applet that triggers the same action, for the layouts that want a
   button.
5. The `WindowPreview` endpoint and renderer from ADR-0119, which upgrades the
   grid tiles from icon-and-title to live thumbnails and also lights up the
   dock's hover previews. No longer a blocker for anything above.

## The controller

`GatherOverviewController` holds the overview's only mutable state — whether it
is open, and how far the grid is scrolled — and makes its one authority
decision. It is Qt Core plus moc: no Qml, no Quick, no display.

Four policies live there because they belong neither in a pure function nor in
QML:

- **A closed overview projects nothing.** Facts arriving while it is closed are
  stored and cost no arrangement, and opening projects the latest of them. A
  session's window churn is free.
- **Opening starts at the top.** A scroll offset remembered from the last time
  the overview was open is never what the user wants from a fresh one.
- **The planner is the only thing that clamps a scroll position.** `scrollBy`
  adds the delta to the offset *the planner last applied* and re-projects.
  Adding to the last *requested* offset instead would let a run of notches at
  the bottom build up an offset far past the end, and the user would have to
  scroll all of it back before the view moved at all.
- **An activation is refused unless the item is in the current projection and
  that projection is interactive.** The surface already draws fenced tiles
  inert, but authority is not a presentation concern: a stale item object held
  across a re-projection, or a QML mistake, must not be able to act. The
  generation reported is always the projection's own, never one the caller
  supplied, since echoing the caller's would defeat the stale-revision
  arbitration it exists for.

The controller resolves `iconName` from `applicationId` through an injected
resolver — the same seam the task-list applet's controller takes — and gives a
container the shell's symbolic container glyph instead, because a container is
not an application. A null resolver leaves the name empty and the surface falls
back to its one-letter badge.

It never acts on a window. `activationRequested` carries the identity and the
generation, and its owner turns that into a task intent with its own authority.

## The surface

`QindaQt.Shell.GatherOverview` draws the projection and decides nothing. Every
frame comes from the projection, which got it from the planner; the surface
translates by its own `origin` so a projection's desktop-logical frames land
correctly on an output that does not start at (0, 0).

It owns no state. A wheel notch becomes `scrollRequested(delta)` — one grid row
of the pitch the planner produced — and the owner adds it and re-projects,
because the planner is the only thing allowed to clamp a scroll position.
A click becomes `activated(item)` carrying the item's `taskId`, `windowId` and
the `generationRevision` it was projected from, so the owner echoes the
generation and stale-revision arbitration can refuse a dead action. Escape and
a click that misses every tile both become `dismissRequested()`.

Three visuals, one per lane: `GatherIconChip` (a round chip, identity only),
`GatherContainerCard` (icon, title, member count, and the container's own
colour on a leading stripe), and `GatherWindowTile` (icon and title, with the
thumbnail area that ADR-0119 will fill). A fenced projection draws every tile
in the disabled role with its handlers off and shows a notice, so a click
cannot look like it worked.

## Related

- [ADR-0119: authenticated window-preview channel](../adr/0119-authenticated-window-preview-channel.md)
- [Window containers](../architecture/window-containers.md)
- [Hybrid topology](../architecture/hybrid-topology.md)
