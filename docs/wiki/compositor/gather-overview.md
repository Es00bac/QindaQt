# Gather overview

Gather brings everything the session holds forward at once, in one
deterministic arrangement, and dismisses without moving anything. It replaces
KWin's own window grid on the top-left screen corner.

**Status: the geometry is implemented and qualified; the surface is not.** What
exists today is `src/hybrid_gather`, the pure planner, plus the release of the
hot corner. What remains is listed under [Remaining work](#remaining-work).

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

## The hot corner

KWin's overview effect reserves the top-left screen corner by default: its
`BorderActivate` default is `ElectricTopLeft` (7), an `IntList`. QindaQt's
session defaults seed `[Effect-overview] BorderActivate=` empty, so no corner
is reserved and the effect keeps its own shortcut. The seed is **missing-only**:
a user who reassigned that corner keeps their choice
(`qindaqt.session-sessiondefaults` pins both halves).

## Remaining work

The surface shows **live window previews**, which the compositor cannot yet
produce. `WindowPreview` on `CompositorShell1` is specified and the whole
shell-side pipeline is implemented, but the renderer behind it is not: the
exported KWin 6.6 headers offer no supported window-texture readback, so
[ADR-0119](../adr/0119-authenticated-window-preview-channel.md) records the
endpoint and renderer as the bounded remaining piece. That is the same blocker
as the dock's hover thumbnails, and it gates this surface too.

In order:

1. The `WindowPreview` endpoint and renderer from ADR-0119.
2. A compositor-side surface that draws this layout, activates whatever is
   clicked, and dismisses on activation or Escape.
3. A `HybridShortcutAction` for it, beside the existing container shortcuts, so
   it has a keyboard trigger.
4. A panel applet that triggers the same action, for the layouts that want a
   button.

A card-and-icon variant — application icon plus title instead of a thumbnail —
needs none of step 1 and would let steps 2 through 4 land first.

## Related

- [ADR-0119: authenticated window-preview channel](../adr/0119-authenticated-window-preview-channel.md)
- [Window containers](../architecture/window-containers.md)
- [Hybrid topology](../architecture/hybrid-topology.md)
