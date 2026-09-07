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

The active tab receives a short accent rule inside its tab rectangle. The rule
is painted from the resolved theme accent, while inactive tabs keep the neutral
surface treatment. Keeping this cue inside the shared row makes the active page
readable without adding a focusable surface or changing native member-frame
ownership. It is the same accent token used by dividers and shared controls, so
light and dark theme changes remain coherent.

Focus cues are a separate snapshot from tab selection. The KWin session samples
the workspace active native window and passes ownership into the immutable plan:
the focused container gets an accent rule on the shared top edge, and the
focused tiled member gets an accent ring only on the paintable side of its
native frame. The ring is intersected with the renderer's transparent-member
clip, so it remains visible when native titles are compact or hidden without
painting client content. An unfocused container keeps the neutral outer frame
even when one of its pages remains selected.

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
separate from tabs and the drag region in left- and right-side layouts. Native member
decorations stay visible as compact 24-logical-pixel strips: they retain normal
member title dragging and per-window controls, while the shared row owns group
controls, tabs, and outer resize. The constraint solver reserves the shared row
once; the chrome plan and scene content frame therefore keep identical bounds.

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
window still blocks the target.

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
neither persists across a compositor restart yet. The menu's remaining live,
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
whole group up to a compact title strip at its current position and width.
No member is minimized (the container never becomes one collapsed dock
entry) and no member's real frame is resized, but every member's content and
pointer input are genuinely hidden while shaded, and only the shared-chrome
anchor stays scene-paintable so the strip remains visible and draggable; see
[Hybrid constraints](hybrid-constraints.md) and
[ADR-0099](../adr/0099-shade-whole-containers-by-hiding-member-content.md).

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
current outside-window focus chosen through Alt-Tab or a panel. A competing
member focus request is rejected after the KWin adapter clears only that
requesting member's native fullscreen, maximize, and quick-tile state and
restores its committed frame. It never reveals a hidden peer, shared chrome, or
changes activation while the accepted focus owner remains active. Minimizing
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

- `KWinChromeManager`'s per-container plan validation compares a plan's tabs
  against the topology's real pages; a shaded plan deliberately omits every
  tab (see below), which always failed that check and discarded the whole
  publication. The shaded branch now validates its own, simpler invariant —
  no tabs, members, or dividers at all — instead of the ordinary plan's
  structural check.
- Group stacking's same-KWin-layer/contiguous-live-stack validation exists to
  protect visible, input-eligible members; it was never given an exemption
  for shade's design of marking every member (anchor included) genuinely
  `Window::isHidden()`. A shaded container now narrows its required members
  to just the content-preserving anchor before that validation runs; its
  hidden siblings are exempt rather than treated as a broken group.
  Unrelated hidden/unmapped windows outside a shaded container are
  unaffected and still fail synchronization normally.
- A shared-chrome press normally raises and activates the group's real
  active-page member so focus/task policy sees a genuine client. For a
  shaded container that representative is the anchor, which is deliberately
  hidden, not a real client; activating it anyway unhid and natively
  detached the container's other, plain-hidden member as a side effect. A
  press on a shaded strip now skips this raise/activate step entirely — the
  strip is already the topmost input target the press just resolved to.

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
the shade/unroll control's checked state and independence from tab/member
geometry, and rename-override text painted into the shared row. Focused
`HybridContainerPlacementController` tests cover the shaded strip's
independent frame tracking (never touching the real committed layout while
shaded, one real reflow on unroll to the original size at the strip's current
position), drag/cancel of the strip, and the maximize/shade mutual exclusion.
`HybridShadeController` fake-platform tests cover the member-hiding
orchestration itself: only the current anchor gets the content-preserving-
visibility treatment, every other member gets plain hide/show, a rejected
shade rolls every already-hidden member back, and a failed unshade still
forgets the container so teardown cannot get stuck. `HybridContainerAppearanceStore`
tests cover name/color normalization, rejection, and per-container
independence. None of this yet covers shading a live grouped window through
a nested KWin session — that is the only way to confirm the real KWin
`WindowItem`/input behavior ADR-0099 depends on.
The nested pointer workflow uses native KDecoration for an ordinary no-modifier
detach; a second workflow unloads the plugin with a process-local group still
owned and verifies restoration. These are functional input proofs, not pixel
screenshots of the live scene item. The qualifying Debug/Release, sanitizer,
live-stress, documentation, and final-audit commands are maintained in the
[testing harness](../development/testing-harness.md) rather than represented by
a frozen test count from the shared registry. Live cross-output DPI migration
and physical input/GPU rendering remain later qualification gates.
