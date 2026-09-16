# ADR-0167: one desktop across every output

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Shell (desktop surface)
- **Supersedes:** None (refines ADR-0125's per-output surfaces and ADR-0161's
  placement store)
- **Superseded by:** None

## Context

ADR-0125 hosts desktop-zone applets on one background-layer surface **per
output**, which is correct: a layer-shell surface belongs to exactly one
output. ADR-0161 then gave desktop icons persistent placement. The placement
store keyed every position by `(screenName, layoutKey)` and each surface
constructed its **own** `DesktopIconLayoutStore` and its own
`DesktopContentsController`.

The result on a two-output session is that both outputs list the same
`~/Desktop` and keep independent positions for the same files. The user
reported exactly that: "the desktop is one thing, not two things just because
there are two monitors, there shouldn't be two sets of desktop icons. Default
to primary display, allow for placement on other displays manually."

Two further defects were found in the same surface while reproducing it:

- **Dragging fought the pointer.** The tile's `MouseArea` is anchored to the
  tile, and the drag delta was measured as `mouse.x - pressX` in tile
  coordinates. Assigning `tile.x` moves that coordinate frame under the
  pointer, so every event after the first reported `delta_n - delta_n-1`
  instead of `delta_n`; the icon lagged, jumped backwards and drifted. This is
  the "icons do not move smoothly" half of the report.
- **A group drag collapsed at an edge**, because each selected tile was
  clamped to the surface independently rather than the group's translation
  being clamped once.

## Decision

### The placement model is desktop-wide

`DesktopIconLayoutStore` stores **global layout coordinates** — the same frame
the compositor uses to lay outputs side by side — keyed by `layoutKey` alone.
An icon therefore has exactly one place on the desktop, whatever the output
topology. The document moves to `schemaVersion: 2` with an `icons` map.

`DesktopSurfaceController` owns **one** store and injects that same object into
every surface, along with the global geometry of every output and the name of
the primary one. Output geometry is republished on hotplug, geometry change and
primary change.

### Each surface draws the icons its output owns

An icon's owner is the output whose rectangle contains the icon's **centre**;
an icon over no output at all (its output was unplugged) falls back to the
primary, so it can never become unreachable. An icon that has never been placed
flows into a default slot on the **primary** output, indexed by its position in
the listing so placing one icon never disturbs where the others sit.

Ownership gates a tile's **visibility**, never the Repeater's model. Filtering
the model by ownership hands the Repeater a fresh JS array on every placement
change, which rebuilds every delegate — destroying the very `MouseArea` holding
the pointer grab in the middle of a drag.

### A drag crosses outputs through a volatile channel

A layer-shell surface cannot paint outside its own output, so a drag across a
seam is continuous only if the surface that will adopt the icon can draw it
while another surface still holds the grab. The store gains
`updateDrag`/`endDrag`/`dragPosition`: the dragging surface publishes live
positions, every surface reads them, and **nothing touches the disk** until the
drop. A crash mid-drag leaves the saved arrangement untouched.

### Motion

The drag delta is measured in the view's coordinate frame, which does not move
with the icon. A group's translation is clamped once against the union of all
outputs, so the set stays rigid at an edge. A dropped icon snaps to the nearest
**free** grid cell on its new output (`snapToGrid`, default true) and glides
there. Placement animations stay off until the surface has a real size and one
layout pass has run, and switch off again across any resize, so icons never fly
in from the window origin at startup or after an output change.

### Migration

A v1 document is retained verbatim until migrated, so an older shell reading
the same file keeps working. `migrateLegacyLayout` keeps the arrangement of one
named output — the primary — offset into the global frame, and drops the other
outputs' placements. A merge is not possible: v1 stored one independent
arrangement per output and the same icon legitimately held two different places.

## Consequences

- One `~/Desktop` entry is drawn by exactly one output, defaults land on the
  primary display, and an icon can be dragged onto any other display and stays
  there.
- Dragging tracks the pointer exactly, group drags keep their shape, and arrange
  animates instead of teleporting.
- Every surface still instantiates a delegate per entry (hidden when not owned).
  That is the price of a stable model; it is bounded by the Desktop directory's
  size, which the store already caps at 4096 entries.
- `DesktopContentsController` is still per-surface, so each output still watches
  the Desktop directory. Rows agree because both list the same directory with
  the same ordering, and the placement model no longer depends on per-surface
  state. Sharing it is a later cleanup, not a correctness fix.
- The primary output comes from the platform's `QGuiApplication::primaryScreen`.
  A session whose compositor-side "primary" differs will flow defaults onto the
  platform's choice; icons remain draggable to the other output.

## Revisit when

- Desktop contents become a single shared controller (fold it into the same
  injection the store uses).
- A display route lets the user choose which output desktop icons default to;
  it should feed `primaryOutputName` rather than adding a second notion.
