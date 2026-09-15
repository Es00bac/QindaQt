# ADR-0162: container aspect-ratio lock

- **Status:** Accepted
- **Date:** 2026-09-15
- **Owners:** Platform (compositor placement policy)
- **Supersedes:** None
- **Superseded by:** None

## Context

Windowed games keep their content inside a fixed aspect ratio and distort or
letterbox when their frame drifts from it. A game grouped as the only item of
a container tab shares the container's outer resize gestures, so nothing
today stops a drag from stretching the group into a shape the game cannot
fill without scaling artifacts. The container model deliberately holds no
outer geometry (see [Window containers](../architecture/window-containers.md));
transient frame state lives with compositor placement policy, and no aspect
concept exists anywhere in the codebase. The split-divider "ratio" is an
unrelated intra-page concept and must stay untouched.

Alternatives considered: enforcing the ratio at the constraint solver
(rejecting committed frames after the drag) makes the live feedback fight the
pointer; delegating to client size hints only works where the game declares
hints; keeping the lock per window is impossible while grouped because
ADR-0117 vetoes native member resize by design.

## Decision

A container may hold one process-local aspect lock through the group context
menu's Aspect Ratio submenu: unlock, lock the committed frame's current
ratio, or one of the fixed presets (16:9, 4:3, 21:9, 1:1).

- The locked value is the ratio of the container's **content area** (the
  outer frame minus the shared chrome: `2*outerBorder` horizontally,
  `2*outerBorder + titleBarHeight` vertically), because that is the
  rectangle grouped content actually occupies.
- While locked, every outer pointer and keyboard resize derives the
  non-leading extent from the locked ratio: width leads on horizontal-edge
  and corner drags, height leads on pure vertical-edge drags. The follower
  edge stays anchored at the baseline edge the drag does not move.
- Minimum frame sizes win over the lock; the clamp cascades so the resting
  frame satisfies the ratio exactly at the clamp point.
- Maximize fills the work area and ignores the lock; restore returns the
  saved frame. Shade rejects resize already, so no shade interaction exists.
- The lock is process-local transient state next to the maximize restore
  frame, cleared with the rest of a container's placement state. It is not
  part of the persistence-neutral `Core::WindowContainer` model and does not
  survive a compositor restart, exactly like rename and color today.

## Consequences

- Grouped games keep their shape under container resizes; the game's own
  size constraints still apply inside the solved tile and center without
  breaking the partition ([Hybrid constraints](../architecture/hybrid-constraints.md)).
- Placement policy owns one new pure function and hash; no topology, solver,
  chrome, or persistence change is required, and divider resizing is
  unaffected.
- The shared-row chrome offsets are duplicated as lock math in placement;
  the two must stay in sync with the chrome metrics (guarded by unit tests
  on the exact derived frames).
- A future session-restore owner must decide whether the lock persists with
  other process-local appearance state; until then restarts drop it.

## Revisit when

- Session restore starts persisting process-local container state (fold the
  lock into that format).
- Independent windows gain a QindaQt resize-interception path (then a lock
  for single windows outside containers can reuse this contract).
