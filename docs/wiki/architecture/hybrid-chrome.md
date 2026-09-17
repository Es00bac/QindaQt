# Hybrid container chrome

`src/hybrid_chrome` owns the compositor-independent geometry, hit-testing, and
Qt painting contract for grouped-window decorations. It does not own KWin
windows, mutate container topology, or decide whether an action is permitted.

## Render-plan boundary

The layout engine accepts one value request containing a shared outer frame,
resolved visual style, logical metrics, stable tab order, member tile frames,
and divider axes. It returns an owned render plan with:

- one compact shared row that combines outer title/tab presentation and outer
  move/resize regions;
- close, minimize, and maximize-or-restore controls for the whole container;
- a native-member-title toggle, a roll-up/unroll (shade) toggle, and one entry
  point for group management;
- a tab strip whose stored vector remains logical order;
- one preserved title-drag region per member tile; and
- visual and deliberately larger pointer hit rectangles for every divider.

Plans contain no `KWin::Window`, decoration, scene, or input-event pointers.
The engine is a reentrant value transformation. `ChromeWidget` copies a plan
and is GUI-thread-only; it exists for settings previews and toolkit-focused
tests, not as the production group surface. `ChromeRenderer` paints the same
plan into the compositor-owned scene image.

Every container receives one identity color. The renderer derives contrast-safe
border, fill, and ink variants from that color plus the resolved theme. The
active tab receives an identity tint and underline, the shared row carries an
always-visible identity stripe, and the focused container strengthens its
identity border and glow. Invalid or absent identity falls back to the theme
accent, preserving compatibility without making the renderer choose policy.

Focus cues are a separate snapshot from tab selection. The KWin session samples
the workspace active native window and passes ownership into the immutable plan:
the focused tiled member gets a stronger identity ring and identity-filled
handlebar only on the paintable side of its native frame. The ring is intersected with the renderer's transparent-member
clip, so it remains visible when native titles are compact or hidden without
painting client content. See
[ADR-0139](../adr/0139-identity-borders-focus-and-rolled-up-badge.md).

The pure hit tester orders window control, outer resize edge, tab, divider,
member title, outer title, then client content. A hit returns a typed action plus
stable ID or logical tab index; it never performs the action. At the production
ordinary-input boundary, the KWin adapter deliberately overrides this order for
the complete native member-title rectangle, including overlap with an enlarged
divider or outer edge. That region must reach KDecoration rather than shared
chrome. The exact-modifier semantic path may still use the pure typed hit.

## Qinda macOS contract

`ChromeStyle::qindaMacOS()` fixes the behavioral presentation while accepting
colors resolved by the theme catalog:

- traffic-light controls are placed on the left in close, minimize, and
  maximize-or-restore order;
- idle traffic lights contain no symbol, while hovering the control cluster
  reveals `x`, `_`, and `[]` action glyphs; and
- tab rectangles are assigned from right to left.

The render plan's tab vector is never reversed. Logical indices, stable IDs,
keyboard traversal, and persistence therefore remain unchanged even though
tab zero is visually rightmost. Standard symbolic styles support either left
or right control placement; the conventional right-side order is minimize,
maximize-or-restore, close.

The shared row reserves left traffic lights, right-to-left tabs, and a minimum
outer drag region without stacking a separate tab strip below it. The two group
controls occupy the side opposite the standard window buttons and remain
separate from tabs and the drag region in left- and right-side layouts. Native
member decorations stay visible as compact 14-logical-pixel handlebars: they
retain a bounded native member drag region and per-window controls, while the
shared row owns group controls, tabs, and outer resize. The constraint solver
reserves the shared row once; the chrome plan and scene content frame therefore
keep identical bounds.

The production Hybrid session currently selects this Qinda macOS style with
the palette resolved by the live AppAppearance projection. Theme changes
therefore update shared chrome and the Qinda KDecoration handoff together; the
pure factory still accepts the resolved palette without owning theme
persistence.

That projection also derives a scoped `QPalette` for QindaQt-owned native menus
and a read-only semantic color map for the bundled KWin switcher. It never
changes KWin's application palette, so third-party decorations, switchers, and
menus retain their selected platform theme. The switcher uses Kirigami colors
when the optional bridge cannot load or before the first confirmed map, and
follows later confirmed changes through the bridge's retained binding. See
[ADR-0092](../adr/0092-project-confirmed-palette-into-compositor-ui.md).

## DPI and output coordinates

Every metric and input rectangle is expressed in device-independent logical
pixels. Changing device pixel ratio does not change layout geometry. The plan
records the ratio and derives a one-physical-pixel border width for raster
painting. A compositor adapter must convert pointer device coordinates to the
plan's logical coordinate system exactly once before hit testing.

On each synchronization the KWin adapter resolves scale from the active-page
representative's current output and rebuilds the plan with the same logical
metrics. Output scale, transform, association, and placement remain compositor
responsibilities; the chrome model must not query global Qt screens or cache an
output object. The virtual matrix covers 1080p, WUXGA, 1440p, fractional 1080p,
and dual common-mode outputs, but a live group moving between heterogeneous
scales remains Platform/Release qualification work.

## KWin and KDecoration composition

Grouped presentation uses two layers, as accepted in
[ADR-0004](../adr/0004-process-local-hybrid-topology.md):

1. the `org.qindaqt` KDecoration3 plugin preserves each application's own
   title bar, caption, and standard close/minimize/maximize actions; and
2. one paint-only KWin scene `ImageItem` per topology container paints the
   shared outer frame, group controls, tabs, member-title alignment, dividers,
   and outer resize edges.

The production item is an ARGB image parented to the topmost active-page
member's `WindowItem`, with a positive child z-order inside that member's stack
slot. The renderer clears the complete outer image and leaves every member
frame transparent, so application content and native KDecoration pixels remain
owned by their real windows. Reanchoring follows the group's topmost member;
failure to resolve a live, paintable anchor hides chrome rather than publishing
a plan-only surface.

Published plans and pointer hit tests stay in global logical coordinates. The
scene image painter receives one localized copy translated by the outer-frame
origin, including buttons, tabs, member/divider geometry, drag regions, and all
three container-control rectangles. This all-fields translation is one bounded
adapter rather than per-field paint logic; otherwise controls remain clickable
through global hit testing while being clipped out of the local image.

Shared chrome creates no production `QWidget`, `QWindow`, internal window,
input mask, focusable surface, or `outputOnly` property. It consequently cannot
be focused, independently raised, or admitted to the managed-client registry.
This scene-resident choice and the superseded internal-window design are
recorded in
[ADR-0005](../adr/0005-scene-resident-hybrid-chrome.md). The separate transient
dock-preview widget remains a frameless, input-transparent internal window; it
sets `outputOnly` on its backing `QWindow` before map because Qt's transparent
input flag alone does not remove an internal window from KWin hit testing.

`HybridChromePlanBuilder` accepts only a validated container plus the copied
committed solution for its active page. It walks the layout tree for stable
member/divider order, verifies scene and chrome content frames agree, and
rejects missing or stale geometry. `KWinChromeManager` stages every fallible
scene-adapter creation before reconciliation, refuses stale revisions or a
changed topology at the same revision, and closes obsolete items only after a
complete replacement plan exists. Stack, activation, registry-output, and
window-state bursts coalesce into next-turn synchronization; only active-page members
participate in chrome ordering. A container ranks at its topmost KWin member,
and reentrant stack signals caused by compacting the group are suppressed.
Registry-output republish resamples committed geometry and live device pixel
ratio without itself claiming an output remap or group reflow.

## Click and drag behavior

The scene item exposes no pointer surface. A pure compositor-side
`HybridChromePointerRouter`, wired into KWin's input filter at Decoration order,
resolves global logical positions against the published plan. KWin's Popup
filter runs first, so the outside press that dismisses a popup cannot also
activate or mutate shared chrome; the QindaQt filter still runs before native
KDecoration starts a title operation. It owns ordinary, unmodified left-button
sequences only for group buttons, group controls, tabs, dividers, outer-title drag, and outer
resize. It pushes hover state back to the scene renderer and, after Qt's
configured drag distance, emits value-only begin/update/commit events with
total displacement from the original press:

- the outer title moves the entire container;
- an outer border resizes it, subject to the current 240x160 minimum outer
  frame and disabled while maximized;
- a tab represents exactly one complete page: it reorders within one container,
  moves to another container's tab target, detaches to empty space, or regroups
  its whole tree with an independent tab target;
- a divider derives a constrained split ratio from its committed split frame
  and publishes `ResizeSplit` on release; and
- clicking without crossing the threshold activates a tab or group window
  action, never both a click and drag.

Member-title and client hits are never consumed by the ordinary router. A plain
member-title drag therefore starts KWin's native KDecoration interactive move.
Member policy observes that start, commits `DetachMember` atomically, and lets
the same native move continue with the restored independent size and pointer
anchor. In the qualified two-member workflow, the dragged window follows the
pointer to its new position while the sibling returns to its exact original
frame and both owners clear. Explicit docking or rearrangement from a member
title remains the exact `Meta+Shift+Left` path. Escape, a lost button release,
target invalidation, or shutdown cancels an owned shared-chrome grab and never
synthesizes a click.

Coordinate hit testing is additionally gated by the live KWin stack at the
point. Chrome is addressable only when its current member anchor is paintable
and no eligible native input owner above that anchor covers the point. Ordinary
clients, internal windows, popups, dialogs, and other transients can therefore
occlude shared chrome without click-through; target discovery stops at the
first real KWin input owner rather than tunneling to a manageable window below.
During an explicit dock drag only the dragged source may be excluded from this
test, allowing it to target chrome beneath itself while every other covering
window still blocks the target. For a tab drag the exclusion covers the dragged
page's complete membership, since the press raises the whole source container
and every one of its page members floats above the destination.

An unmodified right-button press and matching release on the shared outer title,
a page tab, or a grouped native member title opens the same nonblocking QindaQt
group menu. The visible management control in the shared row is the keyboard
and touch-accessible route to that menu.
Its first actions are **Arrange windows**, **Detach active window**, and
**Ungroup**, followed by **Minimize group**. Minimize uses the existing typed
whole-container action, so every member is iconified together and activating
the collapsed task restores the active page without exposing inactive pages.
Next are **Roll up group**/**Unroll group** (the same toggle as the
shared-row shade control; label reflects current state) and **Rename…**,
which opens a synchronous rename prompt after the popup finishes hiding (see
the queued-dispatch guard below) prefilled with the current override, if any.
A **Group Color** submenu offers a fixed curated palette
(`Compositor::containerColorSwatches()`) plus **Default**, radio-exclusive
against the container's current color. Rename and color are process-local
`ContainerAppearance` overrides (`HybridContainerAppearanceStore`, owned by
`KWinHybridSession`): the name replaces the derived title painted into the
shared row's outer-title drag region and the collapsed dock/task entry's
title; the color replaces the shared row's resolved accent (active-tab
underline, rename text, and other accent-derived cues) for that container
only. Neither is part of `Core::WindowContainer`/`TopologyCommand` and
neither persists across a compositor restart yet. Each accepted name or color
edit also invalidates the shell task facts through `shellVisibilityStateChanged`,
so the dock refreshes the override even when native geometry, focus, and task
flags are unchanged. Rejected edits publish no invalidation. The menu's remaining live,
stable-ID actions cover Keep Above, Keep Below, pinning to all workspaces,
individual workspace membership, all or individual activities, and moving the
group to an output. These context actions mutate one current representative;
the queued whole-group transaction described in
[Hybrid constraints](hybrid-constraints.md) adopts the final state atomically.

An ordinary QindaQt-decorated window also owns its chrome right click. Its menu
exposes only actions supported by that live window through KDecoration's public
requests: minimize, maximize or restore, optional roll up or down, workspace
pinning, always on top or below, and close. Commands run after the popup hides
so an action that closes or minimizes the window cannot invalidate the open
menu.

The adjacent title control, or `Meta+Shift+C`, toggles server-drawn member title
bars for the active group. The choice lasts for the compositor session and is
also applied to members later added to that group. QindaQt records each native
member's exact `noBorder` value before changing it and restores that value when
the member detaches, the group is released, or the plugin shuts down. Existing
borderless windows remain borderless. Client-drawn title bars cannot be hidden
through KDecoration and stay under their application's control; the group menu
and keyboard shortcut therefore remain available as recovery controls.

Tab-to-edge drops are intentionally rejected: a tab owns a page tree, and no
typed subtree-as-split command exists. A member dropped on a different page's
tab target extracts only that member into a new page. Exact page-command
semantics are documented in [Hybrid topology](hybrid-topology.md).

Group controls minimize/restore all members together and maximize/restore the
complete outer frame. Close opens a nonblocking **Close All**, **Ungroup**, or
**Cancel** prompt; cancel is the default and escape action. Ungroup uses the
same atomic release path as detach and teardown. The shade control rolls the
whole group up to a compact identity badge at its current position. Its width
shrinks to the metric-derived control/label/tab-pill footprint, capped by the
former container width; the badge uses the container color, and up to eight page pills plus a bounded
overflow counter.

The label is painted at all, and measured rather than fitted into a constant
([ADR-0189](../adr/0189-size-the-rolled-up-badge-to-its-label.md)). Both badge
rectangles are translated into the scene item's image-local space by
`localizeChromeRenderPlan`, which is what puts the label on screen: until that
was fixed, a rolled-up container drew its controls and pills frame-local and
its label at global coordinates, outside the image. One
resolution produces both the text and its width — the text's advance plus the
label rect's 4 px padding on each side, clamped to 48-320 logical pixels — and
both travel on the layout request, so the strip frame the compositor sizes and
the string the badge paints can never disagree. `shade()` reserves the minimum
because the placement controller has neither page titles nor a font; chrome
synchronization then resizes the strip to the measured label, which is also how
a strip grows and shrinks as its foremost page title changes while the
container stays rolled up. The strip keeps its top-left and never outgrows the
frame it was shaded from. When a strip cannot hold both, page pills drop into
the "+N" counter before the label gives up any of its measured width: the label
is the only thing on a rolled-up badge that says which page this is.

That label spends its width on whichever text the
user can actually recognise
([ADR-0168](../adr/0168-a-generated-name-never-displaces-a-real-title.md)): a
container the user renamed reads `<name> · <active page>`, a container that was
never renamed reads the active page title alone, and the stable generated
`Container N`
([ADR-0163](../adr/0163-generated-container-names-for-the-rolled-up-badge.md))
appears only when there is no page title to show. A generated placeholder is
never prefixed to real text - doing so elided the page title out of the badge,
so a title readable on the unrolled row vanished when rolled up. Container
names are not persisted: they live only for the container's process lifetime
(see ADR-0189's consequences for exactly why a restart cannot keep them yet).
No member is minimized (the container never becomes one collapsed dock
entry) and no member's real frame is resized, but every member's content and
pointer input are genuinely hidden while shaded, and only the shared-chrome
anchor stays scene-paintable so the strip remains visible and draggable. The
anchor is held marginally below full opacity so KWin never lets its hidden
client surface occlude the desktop beneath the strip, which would otherwise
leave a stale ghost of client-side-decorated, Electron, Firefox, or borderless
members. KWin's own activation cannot reveal a shaded member: the adapter
re-hides it and hands focus to the next shown window. Member transients hide
with their owner, a closed anchor is replaced by a surviving member, and an
explicit unroll returns focus to the member that held it at roll-up; see
[Hybrid constraints](hybrid-constraints.md) and
[ADR-0099](../adr/0099-shade-whole-containers-by-hiding-member-content.md) and
[ADR-0139](../adr/0139-identity-borders-focus-and-rolled-up-badge.md).

An AppAppearance controller owned once by the compositor projects each confirmed
theme into both grouped chrome plans and native Qinda decorations. The native
handoff is process-local and reapplied after decoration recreation; other
decorations continue to use their KDecoration window palette. See
[ADR-0081](../adr/0081-project-confirmed-appearance-into-window-chrome.md).

Member decoration buttons continue to invoke KWin's per-window actions. An
active-page member's maximize or fullscreen request enters temporary focus mode
without changing the page tree: other group members and shared chrome hide,
and the selected member occupies the group outer frame or KWin fullscreen. The
real maximize bit is cleared for maximize focus; `QindaDecoration` receives a
process-local property so its button shows restore, and a second maximize or a
fullscreen exit restores the exact group baseline. Fullscreen exit preserves a
current outside-window focus chosen through Alt-Tab or a panel. Focus
presentation is tracked per container, so one container's shared chrome hides
and reappears only with its own member's presentation, and a whole-container
action on another container never restores it. A competing member focus
request from the same container is rejected after the KWin adapter clears
only that requesting member's native fullscreen, maximize, and quick-tile
state and restores its committed frame. It never reveals a hidden peer,
shared chrome, or changes activation while the accepted focus owner remains
active. Minimizing
the focused member restores the group then leaves that member minimized;
closing it restores surviving members; native drag commits the topology detach
before clearing temporary focus presentation; shutdown restores focus state
before release-all.

Non-popup dialogs/transients associated with grouped owners remain floating,
never become topology leaves, and follow stable owner-relative geometry,
output, desktops, activities, and stacking. User/client movement updates the
preserved offset. Context following never raises a transient by itself: the
group-stacking policy keeps associated transients above the complete contiguous
member block, so updating a dialog cannot split the group or pull it above an
unrelated active window. A focused dialog remains the valid active window while
scene transactions preserve its opaque KWin focus token.

A shaded container needed three corrections, all live-verified against a real
nested compositor, to keep its published chrome overlay
(`chromeOverlayCount`/`publishedGroupStackingCount`) and strip
click/drag eligibility past the first shade action instead of permanently
dropping to zero:

- `KWinChromeManager`'s per-container plan validation compares the badge pills
  against the topology's real page order and active page, while shaded plans
  deliberately omit member and divider geometry. The shaded branch accepts
  that exact shape instead of either rejecting the pills or admitting hidden
  member geometry.
- Group stacking's same-KWin-layer/contiguous-live-stack validation exists to
  protect visible, input-eligible members; it was never given an exemption
  for shade's design of marking every member (anchor included) genuinely
  `Window::isHidden()`. A shaded container now narrows its required members
  to just the content-preserving anchor before that validation runs; its
  hidden siblings are exempt rather than treated as a broken group.
  Unrelated hidden/unmapped windows outside a shaded container are
  unaffected and still fail synchronization normally.
- A shared-chrome press, dock click, or task-list raise/activate normally
  raises the group's real KWin stack position *and* grants native activation
  to its active-page representative, so focus/task policy sees a genuine
  client. For a shaded container that representative is the anchor, which is
  deliberately hidden, not a real client; activating it anyway unhid and
  natively detached the container's other, plain-hidden member as a side
  effect. `KWinHybridGroupStacking::RaiseActivation` splits the two: a
  shaded container's raise still performs the real z-order raise (so a
  partially occluded strip comes back to front on a press to its exposed
  area) but never activates the anchor, and skips only the
  activation-specific postcondition that assumes a real, activatable
  representative — every dead/layer/contiguous-block check still runs
  identically either way. Every caller that can raise a container (the
  chrome pointer router, dock/task-list window actions) chooses `RaiseOnly`
  for a shaded container and the ordinary activating behavior otherwise.

## Contained window handlebar and wheel roll-up

Container members draw a 14 px handlebar instead of a full native title bar:
title color, a centered grip, miniature stoplights on the effective button
side, and a "more" control that opens the window menu. The compositor's
`memberTitleHeight` equals the handlebar height, so member title regions match
the painted bar. One painter-owned value layout also supplies the live
decoration's control clusters and its button-free native drag rectangle. At
the 108-logical-pixel supported minimum, all four 12 px targets remain in
bounds on either button side and the centered grip contracts to 8 px without
overlap; it grows to 36 px when space permits. Narrower geometry is explicitly
incomplete and paints no grip. A modifier-free vertical wheel over the
container title row, a tab or control, or a member handlebar rolls the
container up (wheel away from the user) or down. Over an ordinary window's
title bar the same wheel rolls that window up to its icon chip (next section),
not into KWin's shade. See
[ADR-0131](../adr/0131-contained-window-handlebar-and-wheel-roll-up.md) and
[ADR-0191](../adr/0191-an-ordinary-window-rolls-up-to-its-icon.md).

## Iconified windows

An independent (ungrouped), server-decorated window rolls up to a floating
application-icon chip when a modifier-free wheel turns away from the user over
its decoration title bar. `KWinInteractionFilter` consumes that axis event at
Decoration order, before native KDecoration, and asks the session to iconify
the topmost input owner under the pointer; popups, panels, dialogs, grouped
members, and client-side decorated windows (which have no title bar the
compositor knows) never qualify. The decoration itself no longer shades on
wheel; KWin's shade remains reachable from the window menu.

Iconified is a session-local state like shade, never a container state and
never persisted. `HybridIconifyController` (pure) records each rolled-up
window's restore frame, chip frame, and focus at roll-up, and knows which
platform treatment to undo; `KWinIconifyPlatform` (`kwinhybridiconify.cpp`)
applies the ADR-0099 technique per window: `Window::setHidden` removes the
window from pointer targeting and the focus chain, `WindowItem::refVisible`
keeps its scene item paintable so the chip parented to it renders, the
window's own container and shadow items are hidden explicitly, its opacity is
held at 0.999 against the occlusion ghost, and its transients hide and restore
with it. Focus moves to the next shown window.

The chip is shared chrome, not a window. `ChromeIconChip` (`src/hybrid_chrome`)
lays out and paints a 48-logical-pixel identity pill anchored at the title
bar's leading edge and clamped into the output: the raised surface with an
identity border (the theme accent; a container-style identity color is
accepted), the application icon KWin resolved for the window, a hover label
with the caption beside the pill (flipped to the left at the output's edge), a
close glyph straddling the pill's top-right edge while hovered, and the
identity glow while hovered. `KWinIconChipPresenter` paints it as a paint-only
scene `ImageItem` parented to the window's own `WindowItem`, exactly like
container chrome ([ADR-0005](../adr/0005-scene-resident-hybrid-chrome.md)):
it creates no `QWindow`, input surface, or managed client, so it is reachable
only through the compositor's own pointer routing. A minimized iconified
window shows no chip until it is unminimized. Chip scene items are released
before a compositor scene teardown and recreated, with the hide treatment
re-applied to the new `WindowItem`, after `compositingToggled(true)`.

Pointer input over a chip goes through `HybridIconChipRouter`, a sibling of
the chrome pointer router with the same modifier semantics. Hit testing walks
KWin's live stack from the top: an exposed chip wins, and any real input owner
above the iconified window covers it; a chip stacked above a container's
anchor makes container chrome yield the point. With no modifier held:

- a left press on the body raises the window (and so the chip) at once;
- a drag past Qt's drag distance moves the chip, and the restore frame moves
  with it, so the window reappears with its title bar under the chip; the
  chip stays inside its output;
- a double-click on the body, or a wheel toward the user, unrolls and
  activates; a wheel away from the user over a chip is swallowed;
- the close glyph closes the window; a right click opens a small
  **Unroll**/**Close** menu whose commands run after the popup hides;
- Escape or a lost release cancels a chip drag.

Any held modifier passes the event through, which is what lets the exact
`Meta+Shift+Left` chord reach the `InteractionController` with the chip as its
source: `KWinInteractionTargetResolver` reports an exposed chip as the hidden
window's own `IconChip` source kind, which docks exactly like a member title
except that a release over nothing still commits (an independent title's
would cancel). On a valid drop the session un-iconifies first
(restore frame, no activation) and then hands the window to the ordinary
docking runtime, so it joins the container as a tab or an edge split at its
restore size; a drop with no target moves the chip to the drop point; Escape
leaves it where it was. See
[Window containers](window-containers.md) "Dock, rearrange, and detach".

KWin activation is an unroll, never a reassert. `Workspace::activateWindow()`
clears `isHidden()`; the platform reports the reveal, the controller forgets
the window and undoes the treatment, and the window reappears at its restore
frame as a real, focusable client. Dock and task-list **Activate**, a client's
own activation token, and an unminimize all unroll. Closing the window while
iconified drops the record and the chip. Compositor shutdown restores every
iconified window at its recorded frame before release.

Task facts publish `iconified` for the window
([Task list](../shell/task-list.md)); the task list keeps the entry, because
the window is still on screen as its chip, and shows it as a rolled hint.
The `Capabilities.hybrid` diagnostics report `iconifiedWindowCount`,
`visibleIconChipCount`, and every `iconifiedWindows` chip and restore frame
([Compositor1 reference](../reference/compositor-control-v1.md)).

Focused rows: `qindaqt.hybrid-chrome-icon-chip` (layout, clamping into
bounds, scaling, hit precedence, icon ink, hover-only glyphs, the placeholder
glyph), `compositor.hybrid-iconify-controller` (platform rollback on refusal,
restore-once, chip relocation within bounds moving the restore frame,
reveal-as-unroll, close, shutdown restore order), and
`compositor.hybrid-icon-chip-router` (raise on press, double-click unroll,
close release, thresholded cumulative drag, context menu, cancel and
invalidation, wheel direction, modifier pass-through). The nested
`compositor.iconify-visibility.*` rows judge the real scene; see the
[testing harness](../development/testing-harness.md) "Iconified window proof".

## Compositor scene restart

Scene image ownership ends before KWin dismantles its `WindowItem` tree.
Direct handlers for `aboutToToggleCompositing` and `aboutToDestroy` first mark
the scene unavailable, invalidate in-flight chrome targets, and clear chrome,
stack anchors, and accessibility roots. Synchronization is suspended until
`compositingToggled(true)`, which occurs after the replacement scene and client
`WindowItem` objects exist; the same immutable topology and committed layouts
then create and anchor fresh image items. KWin's earlier `sceneCreated` signal
is deliberately not used because client items do not yet exist at that point.

The nested development workflow exercises this boundary through the gated
`ReinitializeCompositingForTest` control method. It requires an inactive-to-
active compositor transition and then the same Hybrid container and topology
revision with one visible, anchored scene item. Production rejects that method
with `control-disabled`; the full control contract is in the
[Compositor1 reference](../reference/compositor-control-v1.md).

The separate exact `Meta+Shift+Left` compositor gesture can start on an
independent or grouped title and shares the same semantic docking runtime.
Keyboard modes provide docking/detach, group move, active-divider adjustment,
group resize, and the `Meta+Shift+C` active-group title toggle. Exact bindings
and commit/cancel behavior are documented in
[Hybrid topology](hybrid-topology.md).

## Decoration discovery and defaults

The member decoration is installed at KDecoration3's exported KDE plugin
directory under module ID `org.qindaqt`. On first run, `qindaqt-wm` writes
`[org.kde.kdecoration2] library=org.qindaqt` only when that key is missing. A
user-selected third-party decoration is never overwritten on a later launch.
The Appearance **Windows** destination is the explicit user-facing authority
for changing that selection: it lists installed native and Aurorae choices,
writes only KWin's decoration keys, and requests live reconfiguration
([ADR-0160](../adr/0160-select-installed-kwin-window-decorations.md)). QindaQt
theme and button preferences affect ordinary windows only while `org.qindaqt`
is selected; container chrome remains QindaQt-owned for every selection.

Every non-maximized window using the QindaQt server decoration paints the
theme's `border` color into its reserved one-logical-pixel outer frame. It also
publishes one reusable KDecoration nine-patch shadow derived from the theme's
`surface` color, with a 12-logical-pixel extent. KWin composites that shadow
outside the decoration rather than extending the decoration paint into client
content. Maximized windows retain the flush screen-edge geometry and publish no
outer frame or shadow. Palette, scale, activation, and maximize changes refresh
the native material in place.

The focused plugin test loads the factory and metadata, while the staged-install
test proves that both compositor and decoration artifacts are installed and
that a fresh isolated `kwinrc` receives the default. The nested Hybrid-unload
workflow also requires three mapped probe windows to be server-decorated and
their live KDecoration meta-object class to contain `QindaDecoration`; a silent
fallback to another selected decoration therefore fails the workflow.

Focused QTests cover the native one-pixel frame, bounded nine-patch shadow,
maximized no-outer-material rule, Qinda macOS placement and hover rendering, standard
left/right controls, maximized restore behavior, logical-DPI invariance,
right-to-left visual tabs with stable logical indices, member/divider regions,
hit precedence, malformed geometry, typed widget activation, thresholded drag
lifecycle, cancellation, plan/scene agreement, scene-image lifecycle,
anchor-aware exposure, ordinary router ownership/pass-through, popup ordering,
right-click menu routing, hover forwarding, group-context reconciliation,
focus mode, transient following, stale reconciliation, teardown/rebuild,
the shade/unroll control's checked state, compact badge/pill geometry and
identity paint, and rename-override text painted into the shared row. Focused
`HybridContainerPlacementController` tests cover the shaded strip's
shrunk metric-derived width and independent frame tracking (never touching the real committed layout while
shaded, one real reflow on unroll to the original size at the strip's current
position), drag/cancel of the strip, and the maximize/shade mutual exclusion.
`HybridShadeController` fake-platform tests cover the member-hiding
orchestration itself: only the current anchor gets the content-preserving-
visibility treatment, every other member gets plain hide/show, a rejected
shade rolls every already-hidden member back, a failed unshade still forgets
the container so teardown cannot get stuck, a revealed member gets exactly its
original treatment back, an unroll releases enforcement, a closed anchor is
replaced by a survivor (or reported when none can anchor), and the member
focused at roll-up is remembered. `HybridContainerAppearanceStore` tests cover
name/color normalization, rejection, and per-container independence. The
`compositor.shade-visibility.*` nested rows shade live grouped clients in a
private KWin session and judge captured framebuffer pixels, real client
presses, window inventory, and focus; their fixture matrix and missing
coverage are recorded in the [testing harness](../development/testing-harness.md).
The nested pointer workflow uses native KDecoration for an ordinary no-modifier
detach; a second workflow unloads the plugin with a process-local group still
owned and verifies restoration. These are functional input proofs, not pixel
screenshots of the live scene item. The qualifying Debug/Release, sanitizer,
live-stress, documentation, and final-audit commands are maintained in the
[testing harness](../development/testing-harness.md) rather than represented by
a frozen test count from the shared registry. Live cross-output DPI migration
and physical input/GPU rendering remain later qualification gates.
