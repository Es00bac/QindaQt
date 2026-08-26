# Production panel surfaces

QindaQt's production shell creates one Wayland layer surface for every solved
`(panelId, outputId)` pair. This page owns the boundary between validated layout
profiles, compositor-logical geometry, Qt windows, and LayerShellQt. The
dependency choice is recorded in
[ADR-0007](../adr/0007-layer-shell-panel-surfaces.md).

## Runtime path

`qindaqt-shell` performs a fail-closed startup sequence:

1. load and select a validated profile, theme, applet-manifest catalog, and
   applet capability policy;
2. inventory Qt Wayland outputs by non-empty, unique `QScreen::name()`;
3. start the owner-bound compositor visibility client and select either one
   coherent live generation or the all-visible fallback;
4. require exact compositor/Qt output identity, logical geometry, and scale;
5. solve every profile panel and visibility decision through pure modules;
6. plan a complete backend-neutral surface set; and
7. create hidden QML/layer roles for structural changes, or update mapping and
   reservations in place when every static surface role is unchanged.

An invalid catalog, output inventory, layout, window, or backend preparation
prevents the candidate from replacing the visible set. Startup exits with a
diagnostic; a later hotplug/reconfiguration failure keeps the prior controller
state and reports the error.

The production executable requires Qt's Wayland platform. The separate
`qindaqt-shell-preview` executable remains the offscreen/X11 concept and visual
test surface. Its panel wrapper deliberately approximates a single canvas;
runtime geometry never comes from that QML calculation.

## Ownership

The pure `shell_layout` module owns panel expansion, collision rules, logical
rectangles, and work areas. `shell_surface` then owns:

- translation into anchors, desired size, margins, layer, exclusive edge, and
  exclusive zone;
- selection of one reservation carrier for each output edge;
- deterministic placement order;
- failure-aware controller revisions, in-place runtime transitions, and
  complete-set structural reconciliation;
- the GUI-thread-only Qt output inventory; and
- the private LayerShellQt adapter.

The shell runtime owns profile/theme selection, applet resolution, and its QML
window factory. Runtime QML receives one resolved panel and one theme and
renders only the contents of the already-sized window. It cannot select a
screen, recompute surface policy, or grant applet capabilities. Platform scale
is metadata: Qt and the compositor perform the buffer conversion, while
QindaQt passes the solver's logical size exactly once.

## Anchors and reservations

Fill panels anchor to both along-edge sides. Start, center, and end panels use
only the anchors required to preserve their solved rectangle, with margins
measured relative to the selected output. Top and bottom surfaces are prepared
before side surfaces where KWin's stable layer ordering is needed to preserve
the solver's corner ownership.

`below` uses the bottom layer, `normal` and `above` use the top layer, and
`overlay` uses the overlay layer. Layer shell cannot distinguish QindaQt's
`normal` and `above` values; both retain the profile model's work-area behavior
and the limitation is explicit rather than emulated through KWin-private APIs.

Only the deepest reserving panel on an output edge carries a positive exclusive
zone equal to its own thickness. The protocol includes that carrier's
anchored-edge margin, so their sum reproduces the complete solver-owned depth.
Earlier reserving visuals and all non-reserving surfaces use `-1`, so KWin does
not independently displace them around reservations and does not add a stacked
edge more than once.

## Output changes

The runtime listens for screen addition/removal, geometry, orientation, and
physical-DPI changes and coalesces each signal burst into one complete
inventory/solve/reconcile pass. A disappearing output invalidates its expanded
surface identity; wildcard panels expand again from the new inventory. A
compositor-dismissed layer window is treated as a stale published set even when
the returned output produces an identical plan, so hotplug recovery creates
fresh protocol roles instead of incorrectly eliding the reconciliation.
Intentional autohide is tracked separately from a close event. The backend also
rechecks the selected screen's identity, logical geometry, and device scale
immediately before role preparation and every in-place transition.

`QScreen::name()` is not yet a durable connector migration key. Display
settings and Platform services will later own persistent aliases, lid/hotplug
policy, staged display apply, and exact rollback. Until then, a named profile
target that no longer exists fails rather than silently moving to another
display.

## Current visible scope

This slice makes panels and docks real compositor-managed surfaces and wires
the compositor-driven [panel visibility policy](panel-visibility.md) into the
production runtime. Safe-visible recovery is active, and visibility-only
updates retain existing panel/QML objects. Production panels resolve instances
through the validated manifest and capability policy. The clock is the first
live applet; unresolved instances remain visibly marked static representations.
Preview applet chips remain deterministic visual fixtures. Edge reveal/hold
producers and hide animation also remain acceptance work. Global menu,
notification presentation, live services, applet process hosting, and settings
preview subscription remain in the Shell/customization milestone.

The controller and planner have focused fake-backend tests for valid and
adversarial layouts. Bounded per-role protocol evidence requires the exact
output association, initial role layer, and committed layer, anchor mask,
exclusive edge/zone, and desired size for both default panels under nested KWin
at 1080p, WUXGA, and 1440p. The matching configure must have observed that
committed epoch and then
be acknowledged before a non-null buffer attach and its mapping commit. The
probe snapshots this active mapped epoch while the exact reduced work area is
live; its separate final trace checks identity stability through teardown.
Parser tests reject oversized input, invalid ordering, unmapped acknowledges,
and role/backing-object reuse or destroy ambiguity. Maximized work-area
reduction and restoration are observed separately; screenshots alone are not
evidence for protocol state. The existing matrix proves initial publication,
not live automatic-hide transitions. Those transitions, partial panels, and
heterogeneous multi-output publication remain later matrix rows.
