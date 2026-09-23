# ADR-0243: Keep Gather previews by window identity

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** Shell presentation
- **Refines:** [ADR-0241](0241-capture-gather-window-previews-through-kwin.md)

## Context

The first Gather capture path discarded every preview when the task-list
generation changed. On a busy desktop the generation advances repeatedly even
when the same free windows remain in the grid. Gather then cleared thumbnails
and called KWin's `ScreenShot2.CaptureWindow` again for those same UUIDs. The
user saw previews flash, and a short live D-Bus trace showed repeated captures
of the same windows within seconds.

The task-generation revision protects **activation** from a stale click. A
preview result names a compositor window UUID, and the capture port already
checks KWin ownership and the returned UUID. A generation change alone does
not change which window owns those pixels.

## Decision

Gather keeps each completed preview while its window UUID remains in the
interactive, visible free-window set. A projection with the same UUIDs does
not cancel, request, or clear captures, regardless of its task generation.
When that set changes, Gather cancels pending captures, drops only previews
whose UUIDs departed, and requests snapshots for visible UUIDs without a
completed preview. It accepts an asynchronous result only while Gather is
open and that UUID remains requested and visible. Closing Gather or losing a
usable task-list projection clears all previews and pending work.

The task-list controller still uses the generation revision to arbitrate
activation. This decision changes only snapshot lifetime in the shell.

## Consequences

Stable windows show stable snapshots instead of flashing or repeatedly
capturing on unrelated task updates. Gather does not promise a continuous
video feed; reopening it captures fresh snapshots. Window pixels remain in
shell memory and are removed as soon as their UUID leaves the visible set.
The pure preview ledger has tests for revision churn, departure, and close.
