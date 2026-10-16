# ADR-0384: Degrade the visibility snapshot per window and rescue stranded windows

- **Status:** Accepted
- **Date:** 2026-10-16
- **Owners:** Compositor plugin (shell visibility, task facts, Hybrid placement)
- **Supersedes:** [ADR-0237](0237-the-visibility-snapshot-stays-atomic.md) in part — batch atomicity for the transient window conditions only;
  [ADR-0230](0230-name-and-picture-the-game-behind-a-launcher-class.md) in part — `WineIdentityCache` thread confinement only
- **Superseded by:** None

## Context

[ADR-0237](0237-the-visibility-snapshot-stays-atomic.md) kept the shell
visibility snapshot atomic and made every rejection name its member and cause,
so that one session's log could show whether transient races or producer bugs
dominated. That log now exists (2026-10-16, the owner's laptop):

- 16 rejections for one window at `2560x1408+1536+32`, "outside its output
  `eDP-1` at `1536x864+0+0`": a window left on an HDMI output after it was
  unplugged;
- 19 rejections for a window "active while marked hidden";
- 367 lines of `container 'hybrid-r7-container' has no valid maximize area`
  from a container that had dissolved without its maximize record being
  forgotten.

Each rejection voided the whole batch. The dock went Degraded (task facts
required a valid visibility sample), auto-hide stopped, and window actions were
rejected as stale. The stranded window stayed stranded because Hybrid applied
saved frames (a window's independent restore state, captured when it joined a
container on the HDMI output) after KWin had already evacuated it. The
dissolved container kept its state because only some dissolution paths forgot
it. Wine/Steam identity also re-read `/proc` and the Steam library on every
task-facts sample, on the compositor thread.

KWin evacuates windows from a removed output in `Workspace::updateOutputs()`
and refits special-state windows in `checkWorkspacePosition()`; KDE's
libtaskmanager and GNOME Shell's window tracker update one row at a time. This
decision applies the same ideas inside QindaQt's own boundaries.

## Decision

1. **Transient window conditions degrade only that window.** The producer
   store repairs them per window and reports each repair:
   - a window naming an output this generation does not have, or whose frame
     lies outside its own output, is published on the output its frame
     overlaps most;
   - a window whose frame overlaps no output (stranded) is left out;
   - a window active while minimized or hidden is published as inactive.
   Structural causes (identifiers, duplicates, scope, two active windows, the
   output inventory) still reject the batch with ADR-0237's named messages.
   The published batch is therefore always valid, and the shell keeps
   validating it strictly. The producer logs each degradation once when it
   starts and once when the window is consistent again.
2. **Task facts and the action fence do not depend on the latest visibility
   sample being valid.** They use the last *published* visibility generation
   (`revision > 0`) after closing the invalidation window; a rejected sample
   no longer degrades the dock or rejects every action as stale. A window
   briefly without an output reports the output its frame lies on.
3. **Nothing Hybrid re-applies may land outside every output.** One pure policy
   (`rescuedFrame()`) moves a frame that overlaps no remaining output into the
   work area of the nearest one, keeping its size where it fits. It is applied
   to independent restore states when a container dissolves (a vanished output
   name is dropped), to maximize restore frames and rolled-up strips, to an
   iconified window's unroll frame and a fractional maximize's restore frame,
   and as a final pass after every output change and work-area transition for
   any independent window KWin left outside every output. KWin keeps owning the
   geometry of fullscreen, maximized and quick-tiled windows.
4. **One ledger forgets dissolved containers.** The session reconciles a
   `ContainerLifetimeLedger` against every topology it publishes; any container
   id that vanished, by whatever path, has its placement, shade, minimized and
   appearance state forgotten exactly once.
5. **A hidden container member never keeps focus.** When KWin activates a
   hidden member of a container that is neither rolled up nor in member focus
   mode, focus moves one turn later to that container's visible member (its
   chrome anchor first), or on to the next window when it shows none.
6. **Foreign window identity is resolved once per window, off the compositor
   thread.** The authenticated PID's command line is read once per (window,
   PID, class) and cached until the window closes; the Steam manifest lookup,
   environment read and PE icon extraction run on a worker thread that owns
   the `WineIdentityCache`, and a late Steam name re-samples task facts.

## Consequences

- One stranded or half-updated window can no longer disable auto-hide, the
  dock and window actions for the rest of a session.
- Consumers that count windows see a stranded window disappear from the
  visibility batch until it is rescued (normally within the same output
  change); it stays in task facts, which describe all managed windows.
- Rescue moves rather than resizes, so a layout survives an unplug unless it
  no longer fits; a frame hanging off an edge is the user's arrangement and is
  left alone.
- A per-container store added later must be forgotten in
  `KWinHybridSession::forgetContainerState()`.
- A model-level rescue test stands in for a nested one: the development output
  seam can add and remove a virtual output, but no harness probe can map a
  client onto it, so a window cannot be stranded under nested KWin today.
- Tests: `compositor.shell-visibility-degradation` (stranded window, active
  hidden window, reassignment, mixed batch), `compositor.shell-visibility-snapshot`
  (structural rejections stay named), `qindaqt.shell-visibility-wire-roundtrip`
  (a degraded batch passes the strict consumer), `compositor.output-rescue`,
  `compositor.container-lifetime` (one row per dissolution path) and
  `compositor.foreign-identity-resolver`.

## Revisit when

The structural causes appear in a session log (that is a producer bug to fix,
not a reason to degrade them too), KWin stops evacuating windows from removed
outputs, or the development output seam gains a client-placement probe that
lets the rescue run under nested KWin.
