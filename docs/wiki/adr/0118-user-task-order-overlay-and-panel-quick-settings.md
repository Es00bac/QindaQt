# ADR-0118: User task-order overlay and panel quick-configuration keys

- **Status:** Accepted
- **Date:** 2026-09-10
- **Owners:** Shell / task list, Shell runtime
- **Supersedes:** None
- **Superseded by:** None

## Context

The task list orders rows deterministically (application id, then kind, then
task identity; [task list](../shell/task-list.md)). Determinism fixed two real
defects — iteration-order-dependent generations and unstable keyboard
traversal — but gives users no way to arrange the dock the way they work.
Users asked for drag-and-drop reordering of dock tiles, and, on the same
surface, a right-click panel menu for quick configuration (transparency,
magnification, tile size) plus a shortcut into the Settings Customize route.

Two constraints shaped the design. First, the live profile-binding slice does
not exist: customization applies profiles at the next shell start, so live
panel configuration cannot live in the profile. Second, `Settings1` already
carries an unused `panels.configuration` object key and the shell runtime
already operates a live, scoped Settings1 client, which makes Settings1 the
natural persistence point for both the task order and per-panel quick
settings.

## Decision

1. **User-order overlay.** The canonical order remains the default and the
   fallback. A persisted user order — a validated, deduplicated list of task
   ids bounded by the compositor fact ceiling — takes precedence for the
   displayed order, which is also the keyboard traversal order. Entries not
   named in the overlay keep canonical order behind the overlayed ones, so
   restored or never-moved windows always appear deterministically. The
   overlay is applied to entries and identities in lockstep, and keyboard
   indices are renumbered along the displayed order.
2. **Fenced gestures.** Reordering is a presentation preference: no
   compositor operation, no pending marker. A drag or keyboard move is
   accepted only against the displayed generation revision, and both the
   moved and drop-target tasks must be displayed. The committed order is the
   full displayed order (plus stored ids hidden by scope filters, kept at the
   tail in stored order) and is emitted exactly once per user gesture;
   settings echoes are exact no-ops, so the write path cannot loop.
3. **Settings1 keys.** `panels.configuration` holds:
   - `taskOrder`: the user-order overlay list (shell-wide; today's profiles
     host one task-list instance — per-instance keying is future work if
     multiple task lists ever ship);
   - per-panel entries keyed by panel id: `transparency` (bool),
     `dockZoom` (bool), `dockTileSize` (int 56–64).
   The shell runtime is the write authority for these values; the Settings
   Customize route continues to own profiles. Values are totally decoded —
   malformed containers or out-of-range numbers are dropped whole, never
   partially trusted.
4. **Drag and keyboard parity.** A single passive drag handler per dock strip
   arbitrates: clicks stay with tiles, a threshold-crossing drag shows an
   insertion gap and displaced tiles as transforms only (layout bounds and
   hit targets never move), and the drop commits through the fenced
   reorder intent. The tile context menu exposes "Move left/right" with the
   same fence for keyboard users.

## Consequences

- Dock tiles can be arranged manually and the arrangement survives shell
  restarts; a fresh shell shows the persisted order immediately.
- The canonical order remains the contract for identical fact batches, so
  producer-side determinism tests are unchanged; only the overlay application
  is new surface.
- The shell gains Settings1 write authority for `panels.configuration` only.
  Uncertain writes are surfaced in logs, never replayed; the in-memory order
  stays authoritative for the session.
- Live geometry (position, size, alignment) remains out of scope until the
  live profile-binding slice lands; the right-click menu routes those edits
  to the Customize editor.
