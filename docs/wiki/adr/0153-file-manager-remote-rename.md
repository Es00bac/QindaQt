# ADR-0153: Same-folder remote Rename through KIO's rename()

- **Status:** Accepted
- **Date:** 2026-09-12
- **Owners:** File Manager
- **Supersedes:** ADR-0137's deferral of remote regular-file activation
  semantics for Rename (remote file opening is separately ADR-0152)
- **Superseded by:** None

## Context

ADR-0137 made remote folders read-only: every current-folder mutation
action, including Rename, disables while `remoteActive`. ADR-0151/0152 then
made authenticated folders browsable and their regular files openable, so
Rename remaining disabled was the last daily-use gap on remote folders.
The module must not grow a generic remote-mutation framework, a downloader,
or a credential authority to close it.

KIO already ships the supported facility: `KIO::rename(src, dest)` is a
`KIO::SimpleJob`, so it carries KIO's standard UI delegate and ordinary
authentication prompts work exactly like the ADR-0151 listing backend's.

## Decision

File Manager gains a narrow `RemoteRenamer` seam
(`network/remote_renamer.h`) that `NavigationController` calls for one
operation: renaming a listed child of the currently active remote folder to
a validated sibling name in that same folder.

- `RemoteRenameController` (`network/remote_rename_controller.h`) owns the
  rename state machine — at most one rename in flight, fenced by the listing
  generation captured at dispatch — so `NavigationController` does not
  accumulate it (its 600-line source cap is a project invariant).
  `NavigationController::renameRemoteEntry()` supplies the current
  listing/URL/generation snapshot and surfaces refresh/failure; the
  collaborator validates, dispatches, fences results, and retires the job on
  cancellation.
- Every policy check runs before dispatch: the new name must pass the same
  sibling-name rules as local rename (separators and dot names rejected),
  the source must be a currently listed child of the active directory
  (rejecting unlisted/stale identities, cross-folder URLs, userinfo, wrong
  authority, and unsupported schemes), and at most one rename may be in
  flight. Accepting the dialog's unchanged default name is a true no-op,
  mirroring the local contract.
- `KioRemoteRenamer` is the production adapter on `KIO::rename()`: it
  independently re-validates scheme/authority/parent (defense in depth),
  parents each job to itself so destruction kills pending jobs — and any
  prompt they own — quietly, and maps the result to one `renameFinished`
  per accepted call. `cancel()` retires one generation without touching
  others.
- Results are fenced by the generation captured at dispatch. Success
  refreshes the authoritative listing from the server; the displayed name
  never changes optimistically. Failure surfaces as the controller's
  `launchError` text. Cancellation and replacement navigation (leaving the
  folder or navigating to another remote folder) cancel the in-flight
  rename through `RemoteRenameController::cancelPending()`, and its late
  result is discarded.
- The seam is injected and defaults to null: without it, Rename keeps
  disabling while remote exactly as before, and every existing composition
  and test is unaffected. The stock application composes `KioRemoteRenamer`
  in `main.cpp`, and the AppShell action binding keeps `file.rename`
  enabled remotely only when the seam is present and idle.

## Consequences

- No new link dependency: `KIO::rename()` lives in the KIOCore target
  already linked per ADR-0137/0151.
- Remote copy, cross-folder move, create, write, Trash, restore,
  permissions, search, preview, and mounts gain no authority from this
  slice; each remains an explicit later outcome.
- `qindaqt.file-manager-navigation-controller-network` proves the injected
  dispatch/validation/success-refresh/failure/no-optimistic-rename/
  stale-discard/lifetime wiring; `qindaqt.file-manager-kio-remote-renamer`
  proves the production adapter's scheme/parent boundary and retained KIO
  UI delegate; `qindaqt.file-manager-mutation-action-binding` proves the
  action stays disabled without the seam and enabled with it. All rows are
  no-network.

## Revisit when

Other remote mutations are requested (each needs its own boundary decision,
not a generic framework), or KIO's rename job proves unsuitable in the
QindaQt session; any replacement goes through the same injected seam and
independent review.
