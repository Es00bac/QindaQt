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
2. publish one complete QST-1 generation for the selected theme into the
   shell QML engine, before any panel or hosted applet exists;
3. inventory Qt Wayland outputs by non-empty, unique `QScreen::name()`;
4. start the owner-bound compositor visibility client and select either one
   coherent live generation or the all-visible fallback;
5. require exact compositor/Qt output identity, logical geometry, and scale;
6. solve every profile panel and visibility decision through pure modules;
7. plan a complete backend-neutral surface set; and
8. create hidden QML/layer roles for structural changes, or update mapping and
   reservations in place when every static surface role is unchanged.

An invalid catalog, output inventory, layout, window, or backend preparation
prevents the candidate from replacing the visible set. Startup exits with a
diagnostic; a later hotplug/reconfiguration failure keeps the prior controller
state and reports the error.

The production executable requires Qt's Wayland platform. The separate
`qindaqt-shell-preview` executable remains the offscreen/X11 concept and visual
test surface. Its panel wrapper deliberately approximates a single canvas;
runtime geometry never comes from that QML calculation.

## Desktop control dispatch

The runtime composes desktop controls after the existing applet facades, using
the compositor identity supplied by its startup options to the workspace adapter. It passes one
borrowed `desktopControlsAccess` through the window, panel, zone and chip. The
inert `DesktopControlsAppletComponents` inventory supplies Component definitions
to `BuiltinAppletContent`'s single Loader; it never creates hidden applets. Each
selected control receives the public facade for its function; workspace and
launcher projections are shared where their operations are the same. Dashboard
receives a small view of the status, workspace and launcher facades.
Missing access yields that control's unavailable presentation. Vertical columns
inherit the same forwarding contract from the panel row implementation.

Teardown destroys panel windows first, then desktop controls, then the original
applet compositions whose controllers they borrow. Independent dispatcher tests
cover all thirteen mappings, null access, both orientations, and replacement
without retaining the old control. Controller functionality and capability
checks remain in their owning desktop-controls tests.

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

The desktop surface lays its icons out inside these same reservations. After
every accepted plan the runtime publishes each output's carrier depths (zone
plus anchored-edge margin) to `DesktopSurfaceController`, never the solver's
static work area, so a hidden or overlay panel reserves nothing for desktop
icons either
([ADR-0261](../adr/0261-lay-desktop-icons-out-inside-the-panel-work-area.md)).

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

Panels and docks are compositor-managed surfaces wired to the compositor-driven
[panel visibility policy](panel-visibility.md). Safe-visible recovery is
active, and visibility-only updates retain existing panel/QML objects.
Production panels resolve instances through the validated manifest and
capability policy, then dispatch every audited built-in through its compiled
surface. Preview applet chips remain deterministic fixtures but use the same
icon-first 28-pixel summaries. Launcher, network status, Bluetooth, audio,
clipboard, status-notifier overflow, and unresolved static entries show
icons; power shows an icon plus percentage; task rows show a desktop-entry
icon plus an elided title. Vertical panels suppress those residual labels.
Global menu keeps its provider-owned menu labels, clock remains textual, and
notification center retains its existing compact glyph. Missing icon assets
render typed placeholders without changing the applet's accessible identity.

Each panel creates three disjoint zone viewports, with one configured applet
instance in its selected zone and one lazily constructed implementation.
Unselected orientations, zones, and built-in implementations are not instantiated.
A horizontal panel allocates its zones in reading order of importance: start,
then end, then center. The start zone hosts the active application's menu bar,
whose natural demand is genuinely unbounded, so it is served first and bounded
only by what its neighbours have declared they need; the end zone takes what is
left over after the center's declared minimum; the center zone takes the
remainder, capped at its own demand
([ADR-0188](../adr/0188-serve-the-panel-start-zone-first.md)). A zone's
declared minimum is the sum of its applets' manifest
`sizing.mainAxis.minimum` values plus the row spacing, republished by the
applet resolver as `runtime.mainAxisMinimum` and capped at the zone's current
natural demand so an applet that paints nothing reserves nothing. Long task
strips therefore still cannot reduce small neighboring controls to unusable
slivers, and a wide global menu cannot either. Every allocation is clamped to
what remains, so the three budgets never exceed the panel's content box even
when no zone can have its declared minimum.

Vertical panels and the dock keep the previous rule: small natural demands
first, then the remaining extent shared equally among overflowing zones. They
have no reading order to prefer, and the dock resolves its own tile-fitting
pressure before the zone budget applies. Overflow remains reachable by
scrolling the zone; it cannot paint over neighboring controls. `rows` distributes consecutive
applets across actual horizontal rows (vertical columns on side panels),
sharing the panel cross-axis extent rather than stretching one row.
Zone viewports are the content clip authority; live `AppletChip` content is
not clipped a second time, including the 22-pixel content row of the stock
26-pixel minimal panel. Launcher, Bluetooth, power, audio and clipboard details and status-notifier
operational notices live in focusable `Popup.Window` surfaces, so their text
cannot inflate the panel and their controls remain reachable from a
non-focusable layer-shell panel. Escape closes these detail surfaces through the focused popup window.

Centered bottom panels whose resolved applets request `dockMode` paint a
content-hugging rounded shelf inside their solver-owned surface, which solves
at full edge length. The user preference remains 32–64 logical pixels; the
surface derives one effective tile size for its current horizontal and vertical
budget, proportionally shrinks the shelf down to a 24-logical-pixel floor, and
restores it without persistence churn when space returns. Content beyond that
floor scrolls ([Dock interactions](dock-interactions.md)). The runtime
window factory tracks the QML-painted bounds and applies a `QWindow` input mask
that includes the effective tile's size-derived magnification overscan above
the bottom-aligned shelf. Fixed transparent margins are not made interactive;
only a sub-padding horizontal overrun is added when the magnified icon needs
it. The remaining planned surface passes desktop input through. The mask
returns to the full surface for non-dock panels. QML uses only published QST
colors and the read-only accessibility
projection. A panel material is translucent only when the theme, the
accessibility projection, and the per-panel quick setting all allow it, and
the shell then requests compositor blur-behind (`org_kde_kwin_blur`) for
exactly the painted region ([ADR-0120](../adr/0120-panel-translucency-and-blur.md));
reduced transparency or high contrast produces opaque material.
Edge reveal/hold producers, hide animation, applet process hosting, and
settings preview subscription remain later acceptance work.

Notification windows are separate nonexclusive overlay layer surfaces, not
profile panels or reservation carriers. Their pure planner clamps preferred
popup/center sizes to the compositor's current semantic-primary output logical
geometry, including a 200%-scaled 1080p mode. The runtime reads the ordered
public `Compositor1.Outputs` projection from its exact D-Bus owner, withdraws
the cached order on every invalidation, and accepts a route only when its
`outputGeneration` and complete output-ID set match the accepted shell-
visibility generation and current Qt inventory. The first semantic output is
then resolved by exact `QScreen::name()`; missing authority, cross-generation
state, or a missing Qt screen removes both notification windows rather than
falling back to `QGuiApplication::primaryScreen()`. A popup stack uses a
38-logical-pixel header and up to three
146-logical-pixel cards. That header remains a valid mapped surface with zero
cards while an operation is busy or its bounded error is visible. Current
placement is semantic-primary-output-only and top-right. Popup roles disable
activate-on-show to preserve the non-focus-steal invariant; the center role
requests activation so its QML can seed keyboard focus and expose an Escape
close path. The panel resolution matrix below does not prove notification
mapping, placement, visual appearance, compositor acceptance of that activation
request, focus traversal, keyboard operation, or seat-/active-window placement.
The focused selector and private-bus adapter rows prove transfer, replacement,
removal, exact-owner loss/replacement, and fail-closed mismatches without
starting a compositor; the S3 dual-output row owns the separate live rerun. See
[Notification presentation](notification-presentation.md).

The controller and planner have focused fake-backend tests for valid and
adversarial layouts. Bounded per-role protocol evidence requires the exact
output association, initial role layer, and committed layer, anchor mask,
exclusive edge/zone, and desired size for a deterministic two-panel proof
profile under nested KWin at 1080p, WUXGA, and 1440p. The fixture retains the
QindaQt profile's top-bar and a centered shelf of the fixture's own 52% length but gives both
panels `never` hide mode, preventing the probe's maximized client from racing
the separate intelligent-hide policy. The matching configure must have
observed that committed epoch and then
be acknowledged before a non-null buffer attach and its mapping commit. The
probe snapshots this active mapped epoch while the exact reduced work area is
live; its separate final trace checks identity stability through teardown.
Parser tests reject oversized input, invalid ordering, unmapped acknowledges,
and role/backing-object reuse or destroy ambiguity. Maximized work-area
reduction and restoration are observed separately; screenshots alone are not
evidence for protocol state. The existing matrix proves initial publication,
not live automatic-hide transitions. Those transitions, partial panels, and
heterogeneous multi-output publication remain later matrix rows.

The `qindaqt.panel-geometry-offscreen` regression gate uses production panel
QML with purpose-specific applet doubles. It proves per-instance construction,
disjoint overflowing zones and two-row/two-column placement independently of
compositor mapping. The nested resolution matrix remains the separate surface
and visual qualification boundary.

## Panel hit targets

Two independent host defects made the panel's own controls hard to click, and
both are fixed at the host rather than by enlarging individual buttons.

The first is the zone viewport's attached scroll bars. `PanelAppletRow`
declared `ScrollBar.horizontal`/`ScrollBar.vertical` unconditionally. An
attached `ScrollBar` is a pointer-interactive `Control` spanning the trailing
~10 logical pixels of the zone on its axis, and the Basic style keeps it
mapped and hit-testable while painting it at zero opacity. On the stock 30px
top panel that transparent overlay covered the bottom ~10px of the 26px
applet row and the trailing ~10px of every zone, so it consumed presses aimed
at the hosted control underneath: the lower part of a global-menu word did
nothing and only a click high in the word registered, and status icons lost
their bottom and trailing edges. Each bar now exists only while its own axis
actually overflows and is a non-interactive indicator. The documented
overflow affordance is unchanged — the zone still flicks, wheels, and reveals
a keyboard-focused control inside overflow.

The second is the cross-axis inset. `PanelContent`'s `contentInset` keeps
zones off the panel's along-edge start and end (the left/right margin on a
top/bottom panel, the top/bottom margin on a left/right panel). The
perpendicular inset — the amount trimmed from the panel's own thickness
before a zone's applets are laid out — is now a separate, deliberately
smaller `crossAxisInset`. Reusing the along-axis token for both left only
22px of cross-axis room on the stock 30px panel for hosted controls whose
implicit height is 24-28px, so a control was *taller* than the row hosting
it and its centering expression resolved to a negative offset: the box hung
above and below the row it painted in, spending hit area outside the clipped
row instead of on the label. `crossAxisInset` leaves 26px on that same panel,
at or above those implicit heights. Hosted horizontal controls additionally
fill the row they are given (see `GlobalMenuApplet.qml`'s
`horizontalEntryHeight()`) rather than assuming a fixed 24px box, clamped to
36px so an unusually thick panel does not grow an oversized target.

Evidence: `qindaqt.global-menu-panel-hit-targets-qml-offscreen` hosts the
real global-menu module through `PanelContent` at the default profile's 30px
thickness and opens the menu by clicking the lower edge and the lower
trailing corner of the rendered word, asserting separately that no scroll bar
owns those points. `qindaqt.panel-geometry-offscreen` proves the same
overlay-free hit area for hosted chips in both panel orientations and that an
overflowing zone still shows exactly one, non-interactive, bar on the
overflowing axis only. `qindaqt.desktop-controls-offscreen-workspaces` clicks
the real system-status summary at its lower edge and trailing corner while
hosted at the 26px row a stock panel hands it.

## In-place customization (Meta + right-click)

The desktop is customized where it is. A modifier chord on a panel's own
material, on an applet chip, or on the desktop opens a context menu whose
every entry is one editing gesture followed by one Apply of the user profile,
and an edit mode adds drag handles for longer sessions. The Settings
Customize route stays for previews and for the full property panes; both
surfaces host the same editor domain, so the profile a menu entry persists is
byte-identical to what the route persists for the same intent (see
[Customization editor domain](customization-editor.md#live-host-in-the-shell)
and [ADR-0213](../adr/0213-host-the-customization-editor-live-in-the-shell.md)).

**Chord.** The default chord is Meta + right button; the one Settings1 key
`shell.customization.chord` (`meta-right` or `meta-alt-right`) selects the
alternative. The shell reads it through a purpose-scoped client so an absent
key can never poison the shell's main preference scope. A plain right click
keeps the quick-config menu documented above. Two compositor facts make the
chord deliverable to a focus-less layer-shell panel: KWin sends keyboard
modifiers to the pointer-focused surface, and its `[MouseBindings]`
`CommandAll3` window action (default `Resize`) runs on layer-shell windows
too and would consume the press, so `qindaqt-wm` seeds
`CommandAll3=Nothing` in `kwinrc` (seed-missing only; a user's own binding
wins). On the panel the chord is a modifier branch of the existing right-click
`MouseArea`; on a chip it is a `TapHandler` in `AppletEditHandle` that takes
the exclusive grab on press so the applet beneath never also opens its own
menu, while non-chord presses fall through untouched.

**Menus.** Entry order is a keyboard contract (Down moves one entry, Right
opens a submenu, Enter activates); rows are never hidden or disabled because
QQC2 `Menu` evicts hidden rows and skips disabled ones, which would shift
every later position. The menus open at an explicit point on the panel's far
edge rather than at the pointer: a popup window that maps under the pointer
pre-hovers its first entry. Each menu owns its content view's key handling
(`pinKeyboard`) because the Basic style's view otherwise steps keys itself
whenever its content is a fraction taller than the popup.

| Surface | Entries in order |
| --- | --- |
| Panel (`PanelCustomizeMenu`) | Add applet ▸ (palette admitted by the panel's orientation), Panel ▸ (Edge, Alignment, Auto-hide, Size, Length), Add panel ▸ (edge), Remove panel, Enter/Exit edit mode, Undo, Redo, Open Customize… |
| Applet (`AppletCustomizeMenu`) | Move to start, Move to center, Move to end, Move left, Move right, Move to panel ▸, Remove "‹applet›", "‹applet›" settings ▸ (typed rows from `settingsSchema`: switches, closed choices, bounded integers) |
| Desktop (`DesktopCustomizeMenu`) | Add panel ▸, Change wallpaper…, Desktop icons ▸ (the desktop-icons applet's typed rows), Enter/Exit edit mode, Undo, Open Customize… |

"Move to ‹zone›" appends after the zone's last applet; when the applet already
sits there in the flat list only its zone tag changes (a
`ConfigureAppletSettings` intent), otherwise it is a `MoveApplet` with the
computed anchor. "Move left/right" swaps with the zone neighbour; at the zone
edge it is a harmless no-op. Add applet inserts with the first free
`<plugin>-instance-N` id; Add panel creates `panel-N` (fill, 32px, above) on
the chosen edge through the engine's `AddPanel` command.

**Adoption.** Every accepted entry writes the user store
(`<data>/qindaqt/profiles/<id>.json`, the same file the Settings route
writes) and the shell adopts it immediately through its existing
`adoptLayoutProfile` path, which rebuilds the panel maps in place. The store
watcher would also fire, but not for the very first write on an account whose
store directory did not exist at shell start, so the runtime now also lets
the writable store join the remembered catalog directories as soon as it
exists (the shadowing rule stays: user copy wins on id collision). An
adoption of the profile the live session itself just committed keeps the
session, so Undo walks the history of the whole edit run; a foreign edit (the
Settings route) rebuilds it. Explicit `--profile` still locks adoption for
the process, and an explicit `--profile-dir` excludes the user store, exactly
as before.

**Edit mode.** Meta+Shift+E (a KGlobalAccel action,
`qindaqt_toggle_panel_edit_mode`) or the menu entry toggles it. Every chip
shows a handle (`AppletEditHandle`); dragging one runs the editor's
arm/begin/hover/drop protocol, resolving the target under the pointer through
`PanelLiveCustomization.dropTargetAt`: a zone of this panel with the chip the
drop lands before, or another panel by the solved surface geometry with its
zone chosen by thirds along its main axis. A release over a rejected target
cancels. A Done/Undo bar (`PanelEditBar`) sits at every panel's trailing end
while edit mode is on; it overlays the end zone on purpose because layer-shell
panels take no keyboard focus, so a separate focusable window (and Esc) is
not available -- Done, Meta+Shift+E or the menu entry leave edit mode, and
Esc closes the menus.

Evidence: `qindaqt.live-customization-offscreen` and
`qindaqt.live-customization-edit-mode-offscreen` (chord branch, entry order,
every entry's controller call, handles, bar, drop targets, a handle drag),
`qindaqt.desktop-surface-customize-menu`,
`qindaqt.shell-live-customization-controller` (every action against the real
manifest catalog and a temporary store), and the nested rows
`shell.live-customization.{menu,editmode}.{single-1080p,single-1440p-125}`
(`tests/session/live_customization`), which start the production shell on a
proof profile inside a private virtual KWin, drive the chord and the menus
through the development seat with keys only, assert the persisted profile
after add, move, remove, undo, a desktop add-panel and undo, and a
cross-panel edit-mode drag, capture the open menus and edit mode from the
compositor's framebuffer, and replay the same intents through the Settings
route's own `RepositoryCustomizeEditorHost` (`qindaqt-customize-parity-tool`)
to compare the two profiles byte for byte.
