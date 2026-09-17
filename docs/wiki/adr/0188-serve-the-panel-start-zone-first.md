# ADR-0188: Serve the panel start zone first

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Shell panel surfaces and the global menu applet
- **Supersedes:** the equal-share zone budget of
  [ADR-0118](0118-user-task-order-overlay-and-panel-quick-settings.md) for
  horizontal panels only
- **Superseded by:** None

## Context

`PanelContent.qml` gave every zone whose natural demand exceeded its share
`min(desiredExtent, remaining / zonesLeft)`, walking the zones from the
smallest demand upward. That is a max-min fair split, and it is the right rule
when no zone's content is more important than another's: a long task strip
cannot starve the clock.

It is the wrong rule for the top bar. The start zone hosts the active
application's exported menu bar, whose demand is set by the application and is
genuinely unbounded — an editor with fourteen top-level menus needs fourteen
visible. With all three zones pressed, the split capped the start zone at a
third of the panel: 637 px of a 1920 px panel, and 452 px of a 1366 px one.
The `GlobalMenuApplet` then folded the entries that did not fit into its "+N"
indicator, so the user could not see or reach most of an application's menus.

A second, independent cap made it worse: `maximumVisibleEntries` defaulted to
8, so the ninth top-level menu folded into "+N" even on a panel with hundreds
of spare pixels. The manifest's `sizing.mainAxis.preferred: 520` was not
involved — nothing consumes `preferred` for layout.

## Decision

A **horizontal, non-dock** panel allocates its zones in reading order of
importance, each allocation clamped to what remains:

    start  = clamp(desiredStart, extent − minimumEnd − minimumCenter)
    end    = clamp(desiredEnd,   extent − start − minimumCenter)
    center = clamp(desiredCenter, extent − start − end)

A zone's **declared minimum** is the sum of its applets' manifest
`sizing.mainAxis.minimum` values plus the row spacing, capped at the zone's
current natural demand. The cap matters: an empty live strip collapses its chip
to zero width, and without it an applet that paints nothing would reserve
width from one that does.

The manifest minimum reaches the shell through the applet resolver:
`ResolvedAppletInstance` gains `mainAxisMinimum`, republished in its `runtime`
map as `runtime.mainAxisMinimum`. Until now `sizing.mainAxis.minimum` was
validated and then ignored; this is what makes declaring one meaningful.

**Vertical panels and the dock keep the equal-share split.** Their zones have
no reading order to prefer, and the dock resolves its own tile-fitting pressure
before the zone budget sees it.

`GlobalMenuApplet.maximumVisibleEntries` defaults to the protocol's own
per-item child limit (`menu_limits.h` `kMaxChildrenPerItem`, 128) instead of 8.
The measured fit in `horizontalLimitFor()` is unchanged and remains the thing
that folds entries; the count cap stays a real property for a host that wants a
smaller one.

`centerOffset` is unchanged. It already resolves to the start zone's right edge
when that zone is wide, which is what pushes the center zone right instead of
letting it sit under the menu bar.

## Consequences

- With the stock top-panel profile the start zone's ceiling rises from
  `extent / 3` to `extent − 332 − 48`: 978 px at 1366, 1532 px at 1920,
  2172 px at 2560. A fourteen-entry menu bar fits at 1920 with no "+N".
- The end zone (system status, Bluetooth, power, audio, clipboard, tray,
  notification center, clock) is guaranteed its declared 332 px plus spacing,
  and the center zone its declared minimum, before the start zone takes the
  rest. Nothing the user reads is squeezed to nothing in exchange.
- A panel too narrow for even the declared minimums does not overflow: every
  budget is clamped, the three never exceed the content box, and a zone below
  its minimum scrolls or folds inside its own viewport as before.
- Declaring `sizing.mainAxis.minimum` in a manifest is now load-bearing for
  every applet in a yielding zone. A manifest that declares an unnecessarily
  large minimum takes width from the start zone; one that declares none
  reserves nothing.
- `runtime.mainAxisMinimum` is a new key in the resolved applet map. It is
  additive; a consumer that does not read it is unaffected, and a layout
  document written by hand without it behaves as if the minimum were zero.
- The equal-share rule is still the documented behaviour for vertical panels
  and the dock, so ADR-0118's guarantee holds wherever it still applies.

## Revisit when

- An applet other than the global menu needs unbounded width in a zone that is
  not the start zone.
- A zone gains more than one applet whose demand is application-driven, so
  reading order stops being a sufficient priority.
- `preferred` acquires a real consumer and the manifest can express a target
  rather than only a floor.
