# ADR-0154: Remote New Folder through KIO's mkdir()

- **Status:** Accepted
- **Date:** 2026-09-13
- **Owners:** File Manager
- **Supersedes:** ADR-0137's read-only deferral of remote folder creation
- **Superseded by:** None

## Context

ADR-0137 made remote folders read-only; ADR-0151/0152/0153 progressively
delivered authenticated browsing, remote regular-file opening, and
same-folder Rename. Creating a folder in the current `smb`/`sftp` location
was the next daily-use gap, and the module must not grow a generic
remote-mutation framework, a protocol client, or a credential authority to
close it.

KIO already ships the supported facility: `KIO::mkdir(url)` is a
`KIO::MkdirJob`, so it carries KIO's standard UI delegate and ordinary
authentication prompts work exactly like the ADR-0151 listing backend's.

## Decision

File Manager gains a narrow `RemoteFolderCreator` seam
(`network/remote_folder_creator.h`) that `NavigationController` calls for
one operation: creating exactly one directory in the currently active
remote folder. The shape deliberately mirrors ADR-0153's Rename boundary.

- `RemoteCreateFolderController` owns the create state machine — at most
  one create in flight, fenced by the listing generation captured at
  dispatch — mirroring `RemoteRenameController`.
  `NavigationController::createRemoteFolder()` supplies the active remote
  folder and generation and surfaces refresh/failure; the collaborator
  validates the name, proves the target parent is the active folder,
  dispatches, fences results, and retires the job on cancellation.
- Every policy check runs before dispatch: the name must pass the same
  sibling-name rules as local folder creation and remote rename
  (separators, dot names, and NUL rejected; one-component names only), and
  `NetworkLocation::parentOf(target) == remoteUrl` proves the new directory
  is exactly one level below the active folder — `parentOf()` refuses
  anything at or above the authority root, so no traversal can leave the
  current folder. Overlapping creates are refused.
- `KioRemoteFolderCreator` is the production adapter on `KIO::mkdir()`: it
  independently re-validates the smb/sftp scheme and refuses userinfo
  (defense in depth), parents each job to itself so destruction kills
  pending jobs — and any prompt they own — quietly, and maps the result to
  one `createFinished` per accepted call. `cancel()` retires one
  generation without touching others.
- Results are fenced by the generation captured at dispatch. Success
  refreshes the authoritative listing from the server; no optimistic entry
  is ever displayed. Failure surfaces as the controller's `launchError`
  text. Cancellation and replacement navigation (leaving the folder or
  navigating to another remote folder) cancel the in-flight create, and
  its late result is discarded.
- The seam is injected and defaults to null: without it, New Folder keeps
  disabling while remote exactly as before, and every existing composition
  and test is unaffected. The stock application composes
  `KioRemoteFolderCreator` in `main.cpp`, and the AppShell action binding
  keeps `file.new-folder` enabled remotely only when the seam is present
  and idle.

## Consequences

- No new link dependency: `KIO::mkdir()` lives in the KIOCore target
  already linked per ADR-0137/0151.
- Remote copy, cross-folder move, write, Trash, restore, permissions,
  search, preview, and mounts gain no authority from this slice.
- `NavigationStatus` and its key mapping moved to
  `model/navigation_status.h`, and breadcrumb marshalling moved to
  `model/entry_presentation.h`, so `NavigationController` stays under the
  project's 600-line source cap; both moves are behavior-identical.
- `qindaqt.file-manager-navigation-controller-network` proves the injected
  dispatch/validation/success-refresh/failure/no-optimistic-entry/
  stale-discard/lifetime wiring; `qindaqt.file-manager-kio-remote-folder-creator`
  proves the production adapter's scheme/userinfo boundary and retained KIO
  UI delegate; `qindaqt.file-manager-mutation-action-binding` proves the
  action stays disabled without the seam and enabled with it. All rows are
  no-network.

## Revisit when

Other remote mutations are requested (each needs its own boundary decision,
not a generic framework), or KIO's mkdir job proves unsuitable in the
QindaQt session; any replacement goes through the same injected seam and
independent review.
