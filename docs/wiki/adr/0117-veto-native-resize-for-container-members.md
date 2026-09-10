# ADR-0117: Veto native resize for container members

- **Status:** Accepted
- **Date:** 2026-09-09
- **Owners:** Compositor / window containers
- **Supersedes:** the accepted no-op on grouped-member interactive resizes in the member policy (previously test-encoded)
- **Superseded by:** None

## Context

A window inside a container could be resized directly through KWin's native
interactive resize: decoration edge/corner drag, `Alt`+right-drag, or the
keyboard resize shortcut. The member policy observed
`interactiveMoveResizeStarted` only to drive native move-detach and
deliberately ignored resizes, and every non-maximized member's QindaDecoration
advertised 5 px resize-only borders on all four sides. A native resize
bypassed the tile solver: the container's topology and solved frames stayed
unchanged while the member's real geometry diverged from its tile until the
next divider drag reflowed and snapped it back. See
[Window containers](../architecture/window-containers.md).

The alternative to a veto — letting member geometry float and reconciling it
on `frameGeometryChanged` — would give every member two competing frame
authorities and a permanent divergence/correction loop. The existing model
already treats a grouped member's frame as container-owned; the leak was the
unhandled native path, not the model.

## Decision

A grouped member's frame changes only through tile-border (divider) operations
and container reflow (outer resize, work-area reconciliation, keyboard divider
resize). Enforcement has two halves:

- The KWin member-policy adapter vetoes native interactive resize on owned
  members: when `interactiveMoveResizeStarted` reports a resize (not a move)
  and the toolkit-neutral `HybridMemberPolicy::blocksInteractiveResize`
  predicate holds (owned member, not mid-detach), the adapter cancels the
  operation synchronously. Move-detach behavior is unchanged.
- Grouped members carry no resize-only decoration borders. The compositor
  publishes membership to the decoration through the process-local
  `qindaqtContainerMember` window property, following the existing
  `qindaqtMemberFocusMode` pattern, and clears it on every membership end so
  it cannot go stale.

## Consequences

- The compositor/hybrid member-policy test that encoded the resize no-op is
  replaced by veto-predicate coverage (owned member, non-member, mid-detach),
  and the decoration border matrix is unit-tested.
- Native resize entry points that start an interactive resize on a member —
  including the keyboard resize shortcut — are all covered by the same veto.
- Member-edge drags are not rerouted into the adjacent divider drag; that
  friendlier interaction remains possible future work and is not required for
  correctness.
- X11 `ConfigureRequest` self-resize on owned members keeps its existing
  bounds (KWin's constraint pass plus the rearrange reconciler) and is
  residual scope, not a new guarantee; on Wayland clients cannot self-resize
  a toplevel.
- The fix lives in the KWin plugin and KDecoration, so it takes effect only
  after the package is installed and the session is restarted; the running
  compositor is never restarted over a live session.

## Revisit when

Reconsider if container membership gains a legitimate per-member resize
authority (for example a deliberate free-form tile mode), if KWin changes
`startInteractiveMoveResize` so a synchronous cancel inside the started
signal becomes unsafe, or if member-edge-to-divider drag rerouting lands and
supersedes the plain veto for pointer resizes.
