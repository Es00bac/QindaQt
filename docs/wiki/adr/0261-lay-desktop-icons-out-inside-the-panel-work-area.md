# ADR-0261: Lay desktop icons out inside the panels' work area

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** Shell (desktop surface, panel surfaces)
- **Supersedes:** None (refines ADR-0167's output geometry)
- **Superseded by:** None

## Context

The desktop surface ([ADR-0125](0125-host-desktop-zone-applets.md)) is one
background-layer surface per output, anchored to all four edges with an
exclusive zone of `-1`. It deliberately spans the whole output, behind the
panels, so the wallpaper-level clicks and menus land anywhere.
[ADR-0167](0167-one-desktop-across-every-output.md) gave its icons one
global placement model and injected each output's rectangle. That rectangle
was the full `QScreen::geometry()`, and the default flow used it with a 6 px
margin, so the first row of icons sat under the top bar. In the stock
`qindaqt` layout the first column also sat under the left shelf.

Qt cannot supply the missing fact. On Wayland, `QScreen::availableGeometry()`
does not reflect wlr-layer-shell exclusive zones, and a layer surface is never
told the compositor's work area. The only authority is the shell itself: it
solves the panel layout and publishes each panel's exclusive zone to the
compositor through `PanelSurfaceController`.

Alternatives considered:

- **Read the profile's panels.** Rejected: the solver's static work area
  counts every reserving panel, but at runtime a hidden, auto-hiding or
  dodging panel publishes no zone. Icons would avoid space ordinary windows
  are allowed to cover.
- **Shrink the desktop surface.** Rejected: an exclusive zone or anchored
  margin on the background surface would move the click and menu region and
  interact with KWin's reservation order.
- **Rewrite stored placements when a panel appears.** Rejected: a panel that
  later shrinks or auto-hides could never give the icon its chosen place back,
  and a rewrite races the other outputs' surfaces.

## Decision

- **The work area is the output minus the zones actually requested.** After
  every panel plan `PanelSurfaceController` accepts,
  `ShellRuntimeApplication::reconcileSurfaces()` derives per-output edge depths
  with `PanelReservationInsets::fromPlan()` - each mapped reservation carrier's
  `exclusiveZone` plus its anchored-edge margin, which wlr-layer-shell counts
  inside the zone - and passes them to
  `DesktopSurfaceController::setOutputReservations()`. A rejected plan keeps
  the previous surfaces and therefore the previous depths.
- **Depths, not rectangles, cross the boundary.** The desktop surface cuts the
  work area from the same `QScreen` geometry that places it and publishes it as
  `workArea` inside each `outputRects` entry. It never sees `PanelSurfacePlan`,
  profiles or panel windows; the runtime never sees icon placement. An output
  without depths is all work area, and depths that would leave no room fail
  open to the whole output.
- **Placement uses the work area.** Unplaced icons flow inside the primary
  output's work area. A stored placement is clamped into its owning output's
  work area when it is resolved - an icon saved under the top bar is drawn one
  grid margin below it - and the store is never rewritten by that clamp. A
  live drag's group translation is bounded by the work areas' bounding box, and
  a drop is snapped or clamped into the owning output's work area. Ownership
  (which output draws an icon) still uses the full output rectangle.
- **Changes are live.** Hotplug, profile adoption, a panel added, moved or
  resized, and a hideable panel mapping or unmapping all arrive as a new
  accepted plan; the controller republishes only when the depths change.

## Consequences

- Desktop icons start below the top bar and beside side panels on every
  output, and follow auto-hide exactly as windows do: a panel that reserves
  only while shown (for example a dodging `above`-layer rail) moves icons when
  it appears and disappears.
- A saved position keeps its value on disk. When a panel covers it the icon is
  shown at the nearest edge of the work area, where it can overlap an icon
  saved just below that edge; moving either icon, or Arrange, resolves it.
  Old saves from before this change are shown the same way.
- Until the first plan is accepted (tests, or a shell whose plan failed) the
  surface behaves exactly as before: the whole output is work area.
- The surface, its input region and its popups still span the whole output;
  only icon layout changed.
- Tests: `qindaqt.desktop-work-area-reservations` (depths from real solver and
  planner output, including hidden and overlay panels),
  `qindaqt.desktop-surface-controller` (published `workArea`), and
  `qindaqt.desktop-icon-work-area` (flow, clamp, live reflow, drag bounds and a
  pixel probe of the reserved band).

## Revisit when

- The compositor exposes a per-output work area to layer-shell clients; the
  shell's own derivation could then be checked against, or replaced by, it.
- Desktop popups (the Applications menu's fixed bottom-left placement) need to
  avoid panels too; they should consume the same `workArea`, not a second
  source.
