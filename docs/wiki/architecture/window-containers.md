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

Tabs, leaves, and split dividers are rearrangeable by pointer and keyboard.
Dragging a member's preserved title bar out detaches it, prunes the old tree,
and leaves the remaining topology valid. When a client-side decoration does not
expose a reliable draggable title region, QindaQt supplies a focused/hovered
compositor grip for the same action.

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
- Task lists and Alt-Tab expose one primary container entry with expandable
  member previews and a member-cycling path.
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

## Foundation implementation

`src/core` currently implements the persistence-neutral layout tree and
container value model. It supports page creation and activation, recursive
split insertion, divider-ratio updates, detach/remove normalization,
cross-tree member swaps, page reordering, structural validation, singleton
detection for compositor unwrapping, and schema-versioned JSON round trips.

The model intentionally allows a validated singleton as transaction staging
state. A future compositor transaction coordinator consumes
`singleWindowId()` and must unwrap before publishing session state, preserving
the published-topology invariant above. Cross-container atomic moves, client
size negotiation, geometry, focus, transient handling, and compositor object
lifetimes belong to that upcoming integration layer rather than this pure
model.
