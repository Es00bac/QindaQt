# ADR-0004: Keep Hybrid topology process-local and compose two chrome layers

- **Status:** Accepted
- **Date:** 2026-08-26
- **Owners:** Compositor and Hybrid interaction
- **Supersedes:** None
- **Superseded by:** [ADR-0005](0005-scene-resident-hybrid-chrome.md) for the
  shared-chrome surface only; process-local topology remains accepted

## Context

Hybrid grouping must move members across multiple containers atomically while
KWin objects can map, close, or change state. Pointer motion, divider resizing,
and keyboard docking are latency-sensitive and cannot depend on a round trip to
the shell. The milestone-2 D-Bus bridge owns one container per transaction and
exposes development mutations only in isolated sessions; broadening it into the
production input authority would create transient cross-container ownership and
an unnecessary authorization boundary.

A grouped window also needs two apparently competing behaviors: one outer
title bar must move the complete layout, but every existing member title bar
must remain available for dragging that member back out. A conventional
KDecoration instance is attached to one client and cannot safely become the
owner of other client windows.

## Decision

Keep the authoritative interactive `WindowTopology`, its command coordinator,
and its KWin scene transaction factory inside the `qindaqt_compositor` process.
Input produces typed semantic intents; only commit intents become topology
commands. Each candidate is fully normalized and constraint-solved before one
scene commit changes window state, registry ownership, target frames, and the
published revision. The existing versioned D-Bus bridge remains a separate,
read-only-in-production diagnostic/development surface until a future public
multi-container protocol has an explicit caller policy.

Compose grouped presentation from two layers:

1. `org.qindaqt`, a normal KDecoration3 plugin, preserves each member's visible
   title bar and standard close, minimize, maximize, and restore actions;
2. compositor-owned paint-only shared chrome renders the outer frame,
   container controls, tabs, member-title alignment, and dividers from
   immutable `HybridChrome` plans.

ADR-0004 originally realized the second layer as an input-transparent internal
Qt window. [ADR-0005](0005-scene-resident-hybrid-chrome.md) supersedes only that
surface choice: production now paints one `ImageItem` parented to the topmost
active-page member's `WindowItem`, with no chrome `QWidget`, `QWindow`, native
input surface, or `outputOnly` property. The compositor-side pointer router,
semantic ownership, and process-local topology decision remain unchanged.
Member-title and client hits fall through. KDecoration starts the ordinary
member move; Hybrid observes KWin's interactive-move start, atomically detaches
the member, and lets the same native move continue with restored independent
size. Exact `Meta+Shift+Left` remains the explicit member
docking/rearrangement path.

Qinda macOS presentation uses left traffic lights with cluster-hover glyphs and
right-to-left visual tab placement while keeping logical tab order stable.

## Consequences

- Cross-container moves, close normalization, and plugin-unload release share
  one atomic topology/scene path and do not depend on D-Bus availability.
- The KWin adapter is ABI-coupled to the exact compositor pin; pure topology,
  constraints, input, and chrome modules remain independently testable.
- Two visible chrome layers consume some space, but native member detach and
  standard per-window actions remain KDecoration/KWin behavior.
- Scene chrome has no managed-client identity or native input window. It must be
  reanchored for stack/page/output changes and destroyed synchronously before
  KWin dismantles the parent member `WindowItem`.
- Tree-preserving group move, resize, maximize, and restore use a narrower
  process-local reflow transaction. They retain topology revision while still
  rolling back live window state, focus, target frames, and committed chrome
  layout as one unit.
- A future shell protocol can observe or request topology changes, but it must
  preserve the same typed commands, revision checks, and authorization policy
  instead of exporting KWin pointers or low-level tree edits.

## Implementation status

The pure topology, constraint/restore, input, chrome, and member-decoration
contracts and the production KWin scene-resident chrome, shortcut, preview,
placement, group-context, member focus/transient, and lifecycle collaborator
graph are implemented with focused tests. Nested sessions create a process-local split
with exact `Meta+Shift+Left`, observe its real revision and snapshot, then
detach through a plain native KDecoration drag: the member keeps its size and
follows the pointer while its sibling restores its exact baseline. A second
workflow proves live `QindaDecoration` instances and unload restoration while a
Hybrid-owned group exists. Keyboard modes cover dock/detach, group move,
divider adjustment, and group resize; complete-page commands preserve page
trees; public observation remains read-only.

The Hybrid interaction milestone completed Debug/Release, focused,
bridge-only, sanitizer, live-stress, documentation, and final-audit
qualification. Its authoritative selectors and results are maintained in the
testing harness rather than frozen in this ADR as a test count. Live
heterogeneous mixed-DPI migration, physical input/DRM/GPU
coverage, and output hotplug policy remain Platform or Release boundaries
rather than evidence claimed by this ADR.

## Revisit when

Shared-chrome surface reconsideration is governed by ADR-0005. Reconsider
process-local authority only when a versioned public compositor protocol can
commit multi-container candidates atomically with a documented security model
and measured latency inside the interaction budget.
