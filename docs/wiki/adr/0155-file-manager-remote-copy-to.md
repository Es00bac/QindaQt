# ADR-0155: Remote Copy To through KIO's copy()

- **Status:** Accepted
- **Date:** 2026-09-13
- **Owners:** File Manager
- **Supersedes:** ADR-0137's read-only deferral of remote copy
- **Superseded by:** None

## Context

ADR-0137 made remote folders read-only; ADR-0151–0154 delivered
authenticated browsing, remote regular-file opening, same-folder Rename,
and remote New Folder. Copy To a remote destination was the next daily-use
gap, and the module must not grow a generic remote-mutation framework, a
protocol client, or a credential authority to close it.

KIO already ships the supported facility: `KIO::copy(src, dest)` is a
`KIO::CopyJob`, so it carries KIO's standard UI delegate and ordinary
authentication prompts work exactly like the ADR-0151 listing backend's.

## Decision

File Manager gains a narrow `RemoteCopier` seam (`network/remote_copier.h`)
that `NavigationController` calls for one operation: copying one listed
child of the currently active remote folder to a validated remote
destination folder. The shape mirrors ADR-0153/0154.

- `RemoteCopyToController` owns the copy state machine — at most one copy
  in flight, fenced by the listing generation captured at dispatch —
  mirroring `RemoteRenameController`. `NavigationController::copyRemoteChild()`
  supplies the current listing/URL/generation snapshot and surfaces
  failure; the collaborator validates, dispatches, fences results, and
  retires the job on cancellation.
- Every policy check runs before dispatch: the source must be a currently
  listed immediate child of the active remote directory; the destination
  text must canonicalize (via `NetworkLocation::canonicalize()`) to a
  sibling `smb`/`sftp` folder of the same authority with no userinfo; the
  computed target (`destination/sourceName`) must differ from the source;
  and copying a directory into itself or one of its own descendants is
  refused (a recursion trap KIO must never see).
- `KioRemoteCopier` is the production adapter on `KIO::copy()`: it
  independently re-validates the same-authority/no-userinfo boundary
  (defense in depth), parents each job to itself so destruction kills
  pending jobs — and any prompt they own — quietly, and maps the result to
  one `copyFinished` per accepted call. `cancel()` retires one generation
  without touching others.
- Results are fenced by the generation captured at dispatch. Success
  refreshes the current listing only when the confirmed destination is the
  folder being viewed — and because such a destination computes a target
  equal to the source, the binding refuses it pre-dispatch, so in practice
  no remote copy forces a refresh of the visible folder (no optimistic
  display either way). Failure surfaces as the controller's `launchError`
  text. Cancellation and replacement navigation cancel the in-flight copy,
  and its late result is discarded.
- The seam is injected and defaults to null: without it, Copy To keeps
  disabling while remote exactly as before, and every existing composition
  and test is unaffected. The stock application composes `KioRemoteCopier`
  in `main.cpp`; the AppShell action binding and the destination dialog
  route `file.copy` to the remote path while `remoteActive`, with local
  Copy To behavior unchanged.
- One-child contract at the UI boundary (review repair, 2026-09-13): the
  destination dialog refuses to open or accept a remote Copy To unless
  exactly one current-folder child is selected, so a remote multi-selection
  can never reach the local-only mutation backend's multi-item branch.
  Batch remote copy remains out of scope.
- The shared `operation.cancel` action is truthful about the active owner:
  it enables for an in-flight remote copy as well as for local mutation
  work, and `MutationDialogs` routes it to `NavigationController::
  cancelRemoteCopy()` while a remote copy is busy (quiet KIO kill,
  generation-fenced late result) and to the local mutation backend
  otherwise.

## Consequences

- No new link dependency: `KIO::copy()` lives in the KIOCore target
  already linked per ADR-0137/0151.
- Remote move, write, delete, Trash, restore, permissions, search, preview,
  and mounts gain no authority from this slice; local Copy To/Move To are
  byte-for-byte unchanged.
- The local and remote status-error mappings moved to
  `model/navigation_status.h` so `NavigationController` stays under the
  project's 600-line source cap (behavior-identical); the shared
  refresh/failure bridging for all remote operations now lives in one
  place in the controller.
- `qindaqt.file-manager-navigation-controller-network` proves the injected
  dispatch/validation/success-no-refresh/failure/stale-discard/lifetime
  wiring plus direct user cancellation in place; `qindaqt.file-manager-kio-remote-copier` proves the production
  adapter's scheme/authority boundary and retained KIO UI delegate;
  `qindaqt.file-manager-mutation-action-binding` proves the action stays
  disabled without the seam and enabled with it, and that the shared Cancel
  action tracks an in-flight remote copy;
  `qindaqt.file-manager-remote-copy-guard` proves, through the production
  coordinator → Main.qml → MutationDialogs route, that a remote
  multi-selection fails closed before the local mutation backend and that
  shared Cancel retires the remote copy. All rows are no-network.

## Revisit when

Other remote mutations are requested (each needs its own boundary decision,
not a generic framework), multi-item remote copy is requested (a batch
contract decision), or KIO's copy job proves unsuitable in the QindaQt
session; any replacement goes through the same injected seam and
independent review.
