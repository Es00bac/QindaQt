# Window container model

A QindaQt window container makes multiple application windows behave as one
movable outer window while preserving each member's identity and title bar. A
container has a shared outer title bar and one or more pages; pages are selected
through top-level tabs.

## Domain terms

- **Client**: one compositor-managed application toplevel.
- **Member**: a client placed in a container tile, retaining its application
  title bar when available.
- **Leaf**: a page-tree node referencing exactly one member.
- **Split**: a horizontal or vertical node with exactly two child nodes and a
  bounded divider ratio.
- **Page**: one tab whose root is a leaf or recursive split tree.
- **Container**: outer geometry, output/workspace placement, page list, active
  page/member, and container-wide state.

## Invariants

1. A member appears at most once across all live containers.
2. Every page has exactly one root; a split has exactly two non-null children.
3. Mutation never leaves unary splits, empty pages, duplicate members, or an
   invalid active selection.
4. Divider ratios are finite and normalized within the policy bounds after
   honoring client size constraints.
5. Removing a leaf recursively replaces its parent with the surviving child.
6. A published session topology does not retain a container reduced to one page
   containing one leaf; compositor orchestration unwraps it into an ordinary
   window unless an atomic operation is about to add another member.
7. A mutation either publishes a complete valid topology or publishes nothing.

These invariants belong in the pure domain model and must not depend on QML or
KWin object lifetime.

## Dock, rearrange, and detach

Normal title-bar dragging moves a floating window. With the configured docking
modifier held, hovering over a window or container reveals targets:

- dropping on an edge creates a split at that target. Near a container's own
  edge (a band inside its content frame, 15% of the shorter side and never
  more than 64 logical pixels) the split wraps the whole active page, so the
  dropped window spans the full container beside the existing layout; deeper
  inside, the edge of the member tile under the pointer splits only that tile;
- dropping at the center of a member tile or on the tab strip creates a
  page/tab; and
- dropping outside a valid target leaves the source floating.

A completed drop activates the dropped window — or, when it landed as a
background tab, the receiving container's visible active-page member — so the
container under the pointer ends raised, never the one the window was dragged
out of. A tab drag never resolves its own page's members as targets: the
chrome press raises the source container, and without that exclusion the
dragged page's tiles would shadow the container underneath them.

Tabs, leaves, and split dividers are rearrangeable by pointer and keyboard. A
tab owns one complete page tree: it may reorder, move between containers,
detach as a leaf/window or split/new container, or regroup with an independent
tab target. Dropping a tab on an edge is deliberately rejected until a typed
subtree-as-split operation exists. Dropping one member on another page's tab
target extracts only that member into a new page.

Dragging a grouped member's preserved native title bar without modifiers starts
KWin's ordinary interactive move. Hybrid detaches the member at move start,
prunes the old tree, restores its independent size, and lets KWin continue the
move to the pointer drop. Explicit member docking/rearrangement uses exact
`Meta+Shift+Left` or the keyboard path; shared chrome never intercepts a member
title or client region. Production paints it as a member-anchored KWin scene
item, not a native overlay window.

## Container behavior

- Move, minimize, maximize, pin, workspace assignment, and output movement on
  the outer title bar affect the whole container.
- Whole-container minimize is session state, not derived page state: a
  topology mutation re-plans every container but never resurrects a minimized
  one, and only an explicit restore (task list, group menu, or chrome) brings
  it back. Without this rule a dropped window could target or raise a
  container the user had minimized away.
- A maximized container tracks KWin's current maximize area. A mapped,
  work-area-reserving panel reduces the group frame; releasing that reservation
  for an auto-hidden panel expands the group to the usable output. Revealing or
  changing the panel mode re-resolves the area again without replacing the
  container's independent restore frame. After KWin finishes any work-area
  rearrangement, the compositor also reconciles each owned member's requested
  frame with the container solver's stored target. This prevents KWin's
  ordinary-client constraint pass from moving one member independently when a
  restored floating group makes an auto-hidden panel reserve space again.
  Whole-container maximize, member focus presentation, and minimized groups
  remain under their existing placement or temporary native-frame authority
  until those modes restore the group.
- Closing a member removes only that client. Outer close offers **Close All**,
  **Ungroup**, or **Cancel**.
- A member maximize action temporarily focuses that tile without destroying the
  split tree. Member fullscreen temporarily occupies its output, then restores
  the container exactly. Alt-Tab or an outside panel may take focus while that
  native fullscreen remains active; leaving fullscreen preserves that outside
  focus. Focus presentation is owned per container: each container may present
  one member alone, and a request inside one container never rejects, restores,
  hides, or activates anything in another. Whole-container actions from the
  shared chrome, task list, or group menu (maximize, restore, minimize, shade,
  raise) leave only that container's own focus presentation; a topology
  mutation, which re-plans every group, restores every container first. If
  KWin reports a competing member maximize/fullscreen request while another
  member of the same container already owns temporary focus presentation, the
  compositor rejects that request and restores only the requesting member's
  committed frame/state.
  That one rejection also reactivates the accepted owner, because KWin may have
  activated the requester before reporting its native state change. This is not
  a focus lock: later Alt-Tab, panel, and outside-window focus remain unchanged;
  leaving fullscreen preserves that outside focus.
- Dialogs and other transients float above their owning member and follow the
  container. A crashed member is removed without destabilizing peers.
- Minimum and maximum client sizes constrain divider movement. If the available
  area cannot satisfy all members, the container enters an explicit recoverable
  overflow state rather than clipping silently or corrupting saved ratios.
- Task lists and Alt-Tab expose exactly one primary active-page member as the
  container identity. The dock renders that identity with a stacked-window
  icon in the user-chosen container color, rather than the primary application
  icon. Every other member is suppressed while grouped;
  activating or unminimizing an inactive-page member activates that page before
  it can paint. Each member's independent task/switcher/minimized baseline is
  restored on detach, normalization, close recovery, rollback, or unload.
- Theme decoration tokens may place the outer controls on either side and lay
  tabs left-to-right or right-to-left. This presentation choice never changes
  page order, stable IDs, keyboard traversal, or persistence semantics.
- A container may be rolled up ("shaded") to a compact, still-visible,
  still-movable title strip through the shared-row control or group menu,
  distinct from whole-container minimize/iconify: a shaded container is never
  sent to the dock as one collapsed entry (minimized and shaded are
  independent states), and no member's real frame is ever resized. Instead,
  every member's paint and pointer input is genuinely removed
  (`KWin::Window::setHidden`), while the shared-chrome anchor member is kept
  paintable through KWin's own force-visible scene API so the strip itself
  stays visible and draggable. See
  [ADR-0099](../adr/0099-shade-whole-containers-by-hiding-member-content.md).
- A container may be renamed and given a user-chosen accent color through the
  group menu. Both are process-local presentation overrides (not part of the
  persistence-neutral `Core::WindowContainer` model below): the rename
  replaces the derived title in the shared row and the collapsed dock/task
  entry, and the color replaces the shared row's accent (active-tab
  underline, rename text, focus cue). Neither survives a compositor restart
  yet; see [Hybrid container chrome](hybrid-chrome.md) for the exact
  boundary a future persistence owner reads/writes through.
- The shared title row keeps a visible native-title toggle and group-management
  menu opposite the normal window buttons. `Meta+Shift+C` toggles the same
  active-group choice. Server-drawn member titles restore their exact prior
  border state on detach, release, and compositor shutdown; client-drawn title
  bars remain controlled by their applications.

## Persistence

Snapshots version and persist container geometry, output/workspace identity,
page/tab order, tree topology, ratios, active selections, and application
restore tokens when supported. Output identities may be remapped after hotplug;
topology must remain valid even when geometry cannot be restored exactly.

Every mutation needs invariant tests plus serialize/restore round trips. Display
and interaction coverage belongs in the
[testing harness](../development/testing-harness.md).

## Current implementation

`src/core` currently implements the persistence-neutral layout tree and
container value model. It supports page creation and activation, recursive
split insertion, divider-ratio updates, detach/remove normalization,
cross-tree member swaps, page reordering, structural validation, singleton
detection for compositor unwrapping, and schema-versioned JSON round trips.

The model intentionally allows a validated singleton as transaction staging
state. The compositor bridge consumes `singleWindowId()` and, when a published
group would fall to one member, detaches the survivor in the same scene
transaction. Its atomic `DockWindows` entry point also keeps the initial
singleton private until a two-member split commits. The committed empty
snapshot is a terminal event and the container is then removed.

Two KWin paths now consume this model. The completed Compositor-MVP D-Bus
bridge remains per-container and development-only for mutation. The
process-local Hybrid runtime owns session-wide cross-container commands,
recursive size negotiation and overflow, full independent-window restore
values, active/inactive page state, focus selection, group placement, and
shared scene-resident chrome composed with the QindaQt KDecoration.

The production Hybrid graph implements pointer and keyboard docking/detach,
complete-page and one-member reorganization, divider and complete-group geometry
controls, member maximize/fullscreen focus mode, focus-safe minimize/close/native
detach, transient following, lifecycle focus/stack/output synchronization,
collapsed task/switcher identity, Close All/Ungroup/Cancel, virtual
accessibility trees, and readable process-local snapshots. Native minimize of
any visible grouped member minimizes the complete container; restoring exposes
only its active page. Nested workflows create and detach a group through the
real input paths, require live QindaQt decorations, and verify exact restoration
when a grouped plugin unloads. Final qualification evidence is maintained in
the [testing harness](../development/testing-harness.md).

Persisted login-session restore, mixed-DPI output migration, and physical
hardware qualification remain later desktop work. Current and intended behavior
are separated in
[Hybrid topology](hybrid-topology.md), [Hybrid constraints](hybrid-constraints.md),
[Hybrid chrome](hybrid-chrome.md), and the
[testing harness](../development/testing-harness.md).
