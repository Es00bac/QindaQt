# Hybrid container chrome

`src/hybrid_chrome` owns the compositor-independent geometry, hit-testing, and
Qt painting contract for grouped-window decorations. It does not own KWin
windows, mutate container topology, or decide whether an action is permitted.

## Render-plan boundary

The layout engine accepts one value request containing a shared outer frame,
resolved visual style, logical metrics, stable tab order, member tile frames,
and divider axes. It returns an owned render plan with:

- one shared outer title bar and outer move/resize regions;
- close, minimize, and maximize-or-restore controls for the whole container;
- a tab strip whose stored vector remains logical order;
- one preserved title-drag region per member tile; and
- visual and deliberately larger pointer hit rectangles for every divider.

Plans contain no `KWin::Window`, decoration, scene, or input-event pointers.
The engine is a reentrant value transformation. `ChromeWidget` copies a plan
and is GUI-thread-only; it exists for settings previews and toolkit-focused
tests, not as the production group surface. `ChromeRenderer` paints the same
plan into the compositor-owned scene image.

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

The production Hybrid session currently selects this Qinda macOS style with
the built-in Qinda palette. The pure factory accepts a resolved palette, but
wiring live theme changes into shared chrome and the KDecoration plugin belongs
to the Shell/customization theme integration; the current member decoration
uses the same checked-in colors directly.

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
sequences only for group buttons, tabs, dividers, outer-title drag, and outer
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

An unmodified right-button press and matching release on the shared outer title
opens a nonblocking group menu. Its live, stable-ID actions cover Keep Above,
Keep Below, pinning to all workspaces, individual workspace membership, all or
individual activities, and moving the group to an output. Native member-title
right clicks remain KWin's per-window menu. The group menu mutates one current
representative; the queued whole-group context transaction described in
[Hybrid constraints](hybrid-constraints.md) adopts the final state atomically.

Tab-to-edge drops are intentionally rejected: a tab owns a page tree, and no
typed subtree-as-split command exists. A member dropped on a different page's
tab target extracts only that member into a new page. Exact page-command
semantics are documented in [Hybrid topology](hybrid-topology.md).

Group controls minimize/restore all members together and maximize/restore the
complete outer frame. Close opens a nonblocking **Close All**, **Ungroup**, or
**Cancel** prompt; cancel is the default and escape action. Ungroup uses the
same atomic release path as detach and teardown.

Member decoration buttons continue to invoke KWin's per-window actions. An
active-page member's maximize or fullscreen request enters temporary focus mode
without changing the page tree: other group members and shared chrome hide,
and the selected member occupies the group outer frame or KWin fullscreen. The
real maximize bit is cleared for maximize focus; `QindaDecoration` receives a
process-local property so its button shows restore, and a second maximize or a
fullscreen exit restores the exact group baseline. A competing member focus
request is rejected. Minimizing the focused member restores the group then
leaves that member minimized; closing it restores surviving members; native
drag commits the topology detach before clearing temporary focus presentation;
shutdown restores focus state before release-all.

Non-popup dialogs/transients associated with grouped owners remain floating,
never become topology leaves, and follow stable owner-relative geometry,
output, desktops, activities, and stacking. User/client movement updates the
preserved offset. Context following never raises a transient by itself: the
group-stacking policy keeps associated transients above the complete contiguous
member block, so updating a dialog cannot split the group or pull it above an
unrelated active window. A focused dialog remains the valid active window while
scene transactions preserve its opaque KWin focus token.

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
and group resize. Exact bindings and commit/cancel behavior are documented in
[Hybrid topology](hybrid-topology.md).

## Decoration discovery and defaults

The member decoration is installed at KDecoration3's exported KDE plugin
directory under module ID `org.qindaqt`. On first run, `qindaqt-wm` writes
`[org.kde.kdecoration2] library=org.qindaqt` only when that key is missing. A
user-selected third-party decoration is never overwritten on a later launch.

The focused plugin test loads the factory and metadata, while the staged-install
test proves that both compositor and decoration artifacts are installed and
that a fresh isolated `kwinrc` receives the default. The nested Hybrid-unload
workflow also requires three mapped probe windows to be server-decorated and
their live KDecoration meta-object class to contain `QindaDecoration`; a silent
fallback to another selected decoration therefore fails the workflow.

Focused QTests cover Qinda macOS placement and hover rendering, standard
left/right controls, maximized restore behavior, logical-DPI invariance,
right-to-left visual tabs with stable logical indices, member/divider regions,
hit precedence, malformed geometry, typed widget activation, thresholded drag
lifecycle, cancellation, plan/scene agreement, scene-image lifecycle,
anchor-aware exposure, ordinary router ownership/pass-through, popup ordering,
right-click menu routing, hover forwarding, group-context reconciliation,
focus mode, transient following, stale reconciliation, and teardown/rebuild.
The nested pointer workflow uses native KDecoration for an ordinary no-modifier
detach; a second workflow unloads the plugin with a process-local group still
owned and verifies restoration. These are functional input proofs, not pixel
screenshots of the live scene item. The qualifying Debug/Release, sanitizer,
live-stress, documentation, and final-audit commands are maintained in the
[testing harness](../development/testing-harness.md) rather than represented by
a frozen test count from the shared registry. Live cross-output DPI migration
and physical input/GPU rendering remain later qualification gates.
