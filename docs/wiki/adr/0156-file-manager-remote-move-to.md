# ADR-0156: Remote Move To through KIO's move()

- **Status:** Accepted
- **Date:** 2026-09-13
- **Owners:** File Manager
- **Supersedes:** ADR-0137's read-only deferral of remote file move
- **Superseded by:** None

## Context

ADR-0155 closed the remote Copy To gap with `KIO::copy()`. Moving one listed
remote child to another folder on the same SMB/SFTP authority was the next
daily-use gap. A remote move is qualitatively different from a copy: it is
**destructive**. KIO's move job may delete the source at the server even
when the job ultimately reports a failure, so any validation weakness is not
recoverable by re-running the operation, and the module must not grow a
generic remote-mutation framework, a protocol client, or a credential
authority to close the gap.

KIO already ships the supported facility: `KIO::move(src, dest)` returns a
`KIO::CopyJob`, so -- exactly like `KIO::copy()` -- it carries KIO's
standard UI delegate and ordinary authentication prompts work like the
ADR-0151 listing backend's.

## Decision

File Manager gains a narrow `RemoteMover` seam (`network/remote_mover.h`)
that `NavigationController` calls for one operation: moving one listed child
of the currently active remote folder to a validated remote destination
folder. The shape mirrors ADR-0155.

- `RemoteMoveToController` owns the move state machine -- at most one move
  in flight, fenced by the listing generation captured at dispatch --
  mirroring `RemoteCopyToController`. `NavigationController::moveRemoteChild()`
  supplies the current listing/URL/generation snapshot and surfaces
  refresh/failure; the collaborator validates, dispatches, fences results,
  and retires the job on cancellation. Because the move is destructive,
  every policy check runs before dispatch and the visible listing never
  anticipates the result.
- Every pre-dispatch check mirrors Copy To: the source must be a currently
  listed immediate child of the active remote directory; the destination
  text must canonicalize (via `NetworkLocation::canonicalize()`) to a
  sibling `smb`/`sftp` folder of the same authority with no userinfo; the
  computed target (`destination/sourceName`) must differ from the source;
  and moving a directory into itself or one of its own descendants is
  refused (a recursion trap KIO must never see).
- `KioRemoteMover` is the production adapter on `KIO::move()`: it
  independently re-validates the same-authority/no-userinfo boundary
  (defense in depth), parents each job to itself so destruction kills
  pending jobs -- and any prompt they own -- quietly, and maps the result to
  one `moveFinished` per accepted call. `cancel()` retires one generation
  without touching others.
- **Success semantics differ from Copy To and are the reason this is its own
  controller rather than a flag on the copy path**: a confirmed move always
  removes its source from the folder being viewed, so `refreshRequested()`
  fires after every confirmed success (Copy To refreshes only when the
  destination is the visible folder, which its same-target refusal makes
  unreachable today). A confirmed move's refresh is also the authoritative
  answer when the server deleted the source mid-move and then reported a
  failure -- the refreshed listing, never an optimistic disappearance, is
  the truth the user sees.
- Results are fenced by the generation captured at dispatch. Failures
  surface as the controller's `launchError` text. Cancellation (the shared
  `operation.cancel` action, replacement navigation, or leaving remote)
  retires the in-flight move quietly and its late result is discarded.
- The seam is injected and defaults to null: without it, Move To keeps
  disabling while remote exactly as before, and every existing composition
  and test is unaffected. The stock application composes `KioRemoteMover`
  in `main.cpp`; the AppShell action binding keeps `file.move` enabled while
  `remoteActive` only with a mover injected and no move in flight (a second
  destructive move can never overlap), and the destination dialog routes the
  accepted move to the remote path while local Move To behavior is
  unchanged.
- One-child contract at the UI boundary, extending ADR-0155's review repair
  to Move To: the destination dialog refuses to open or accept a remote
  Move To unless exactly one current-folder child is selected, so a remote
  multi-selection can never reach the local-only mutation backend's
  multi-item branch. Batch remote move remains out of scope.
- The shared `operation.cancel` action is truthful about the active owner:
  it enables for an in-flight remote move as well as a copy or local
  mutation work, and `MutationDialogs` routes it to
  `NavigationController::cancelRemoteMove()` while a remote move is busy.

## Consequences

- No new link dependency: `KIO::move()` lives in the KIOCore target already
  linked per ADR-0137/0151.
- Remote write-in-place, delete, Trash, restore, permissions, search,
  preview, and mounts gain no authority from this slice; local Move To is
  byte-for-byte unchanged.
- `NavigationController` now reaches the project's 600 non-blank source
  line hard cap (it carried a 500-line decomposition-review warning
  already). Splitting was considered and rejected: the four per-operation
  bridges (rename, create, copy, move) are three-to-nine-line wiring blocks
  whose only shared structure is "connect busy/refresh/failure to one of
  three existing hooks"; extracting them into a generic remote operation
  dispatcher would create exactly the generic remote-mutation framework
  this ADR (and ADR-0155) reject, and would couple four independently
  reviewed operation boundaries through one new abstraction. The redundant
  per-seam availability emits in the remote leave/replace paths were
  consolidated to unconditional emits (the binding re-sync is idempotent)
  to keep the file at the cap; the per-operation controllers already carry
  the state machines, and the bridge remaining in `NavigationController`
  is the cohesive minimum.
- `qindaqt.file-manager-remote-move-dispatch` proves the injected
  dispatch/validation, always-refresh-on-confirmed-success, failure,
  stale-discard, direct user cancellation, and destruction wiring against
  fakes; `qindaqt.file-manager-kio-remote-mover` proves the production
  adapter's scheme/authority boundary and retained KIO UI delegate;
  `qindaqt.file-manager-remote-copy-guard` now also proves, through the
  production coordinator -> Main.qml -> MutationDialogs route, that a remote
  multi-selection Move fails closed before the local mutation backend, that
  one selected child routes to the injected mover, and that shared Cancel
  retires the remote move; `qindaqt.file-manager-mutation-action-binding`
  proves the action stays disabled without the seam, enables with it, and
  that the shared Cancel action tracks an in-flight remote move. All rows
  are no-network.

## Revisit when

Other remote mutations are requested (each needs its own boundary decision,
not a generic framework), multi-item remote move is requested (a batch
contract decision), or KIO's move job proves unsuitable in the QindaQt
session; any replacement goes through the same injected seam and
independent review.
