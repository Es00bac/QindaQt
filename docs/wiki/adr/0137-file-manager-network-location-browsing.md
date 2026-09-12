# ADR-0137: File Manager network-location browsing (S5)

- **Status:** Accepted
- **Date:** 2026-09-12
- **Owners:** File Manager
- **Supersedes:** None
- **Superseded by:** None

## Context

File Manager's `DirectoryLister` contract ([Module boundaries](../architecture/module-boundaries.md))
was local-filesystem-only through S1–S4: `LocalDirectoryLister` performs one
bounded synchronous `QDir` read, and `NavigationController` never blocked
longer than that read. [File Manager](../apps/file-manager.md) documented S5
("network locations (SMB/SFTP-style browsing) behind a new injected backend
seam and ADR") as the deferred next slice, since a real network location
cannot be listed synchronously and cannot reuse the local mutation/preview/
Trash/search authorities that a `DirectoryEntry`'s local identity fields
imply.

The project already accepts KIO as a dependency (`src/workspaces_apps` links
`KF6::KIOGui` for `KIO::ApplicationLauncherJob`), so KIO's SMB/SFTP KIO
slaves are the natural production backend rather than a hand-rolled network
protocol client.

## Decision

File Manager gains a narrow, injected `NetworkDirectoryBackend` seam
(`src/apps/file_manager/network`) that `NavigationController` uses exactly
the way `SearchController` already uses its own worker: asynchronous,
generation+URL fenced, GUI-thread-published results.

- `NetworkLocation` (`network_location.h`) is the sole allowlist/
  canonicalization gate: only lower-cased `smb`/`sftp` schemes with a
  non-empty host and no embedded userinfo are accepted; a `.`/`..` path
  segment is collapsed/refused so two spellings of the same folder always
  canonicalize identically and no path can escape the authority root.
  `NavigationController::navigateTo()` routes through it; anything it
  reports as `Local` (including every ordinary absolute path, and any other
  scheme) takes the existing, byte-for-byte-unchanged local path.
- `NetworkDirectoryBackend` (`network_directory_backend.h`) is the abstract
  interface: `requestListing(generation, url)` / `cancel(generation)`,
  publishing exactly one `listingReady(generation, url, NetworkListingResult)`
  per accepted request. `NetworkListingResult` carries a typed
  `NetworkListingError` (`Unavailable`, `AuthenticationRequired`,
  `PermissionDenied`, `NotFound`, `Transport`, `Unknown`) instead of a
  generic failure. `NavigationController` owns the fencing: a callback whose
  generation or URL no longer matches the current remote location is
  silently discarded, mirroring `SearchController`'s own token discipline.
- `KioNetworkDirectoryBackend` is the production adapter: it confines every
  `KIO::ListJob` it starts, independently re-validates the scheme/policy
  boundary before ever calling `KIO::listDir()`, bounds the result to
  `NetworkLocation::maximumEntries`, and clears each job's UI delegate and
  delegate extension before starting it — a slave that would otherwise
  prompt for a password or a mount instead fails the job with a typed
  `AuthenticationRequired`/`Unavailable` result. It owns no wallet/keyring
  handle and persists no credential; a location string carrying one is
  refused by `NetworkLocation::canonicalize()` before it ever reaches this
  adapter.
- Remote `DirectoryEntry` rows reuse the existing local entry shape (name,
  path, isDirectory, size, lastModified) but leave every local-identity
  field (device/inode/identitySize/modifiedNanoseconds/mode) at zero. This
  is deliberate, not an oversight: `MutationController`'s identity check
  would refuse a zero-identity request anyway, and `NavigationController`
  additionally exposes `remoteActive` so mutation/preview/recursive-search/
  bounded-local-launch entry points disable while a remote folder is
  current. Remote regular-file activation reports a truthful typed
  "not supported yet" `launchError` instead of downloading, executing, or
  attempting a local launch of a URL-shaped path.
- The Places sidebar's "Network" entry exposes the *route* to browsing
  (it opens the editable location bar) without pretending a connection
  exists; it carries no path and is never itself a navigable location.

## Consequences

- File Manager gains a private `KF6::KIOCore` dependency for the network
  module only; no public header exposes a KIO type, and the module-
  boundaries row documents the addition.
- Undo, Restore Last, and Empty Trash stay independent of the current
  folder (they already operate on the local Trash/last-operation history),
  so they remain enabled while browsing a remote location; every
  current-selection mutation action (new folder, rename, cut/copy/paste,
  move, trash, properties) disables instead.
- Remote directory navigation, typed async failures, generation/URL
  fencing, truncation, and the disabled local-only actions are covered by
  `qindaqt.file-manager-network-location`,
  `qindaqt.file-manager-navigation-controller-network`, and
  `qindaqt.file-manager-kio-network-backend` against fakes/synthetic URLs
  and a job-creation test seam; none of these rows make a DNS lookup,
  socket connection, or real KIO network request.
- Remote file opening/mutation, mounts, per-volume Trash, a credential/
  password UI, and installed/live network-share interoperability remain
  explicit later work; this slice's evidence is source/unit-level only.

## Revisit when

A later slice needs remote file opening (requires a download/cache policy
and a new ADR), mount-based access, or a credential-entry UI — each is a
materially different authority boundary from read-only asynchronous
listing and should not silently grow this seam.
