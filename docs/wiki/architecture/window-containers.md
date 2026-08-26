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

- dropping on an edge creates a split at that target;
- dropping at the center or tab strip creates a page/tab; and
- dropping outside a valid target leaves the source floating.

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
- Closing a member removes only that client. Outer close offers **Close All**,
  **Ungroup**, or **Cancel**.
- A member maximize action temporarily focuses that tile without destroying the
  split tree. Member fullscreen temporarily occupies its output, then restores
  the container exactly.
- Dialogs and other transients float above their owning member and follow the
  container. A crashed member is removed without destabilizing peers.
- Minimum and maximum client sizes constrain divider movement. If the available
  area cannot satisfy all members, the container enters an explicit recoverable
  overflow state rather than clipping silently or corrupting saved ratios.
- Task lists and Alt-Tab expose exactly one primary active-page member as the
  container identity. Every other member is suppressed while grouped;
  activating or unminimizing an inactive-page member activates that page before
  it can paint. Each member's independent task/switcher/minimized baseline is
  restored on detach, normalization, close recovery, rollback, or unload.
- Theme decoration tokens may place the outer controls on either side and lay
  tabs left-to-right or right-to-left. This presentation choice never changes
  page order, stable IDs, keyboard traversal, or persistence semantics.

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
