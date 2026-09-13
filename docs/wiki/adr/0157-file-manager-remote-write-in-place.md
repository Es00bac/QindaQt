# ADR-0157: Remote write-in-place through the session KIOFuse service

- **Status:** Accepted
- **Date:** 2026-09-13
- **Owners:** File Manager
- **Supersedes:** ADR-0152's incidental write-back behavior (a remote file's
  edits reached the server only when the handler happened to write back)
- **Superseded by:** None

## Context

ADR-0152 opened a listed remote regular file by handing its raw `smb`/`sftp`
URL to `KIO::OpenUrlJob`. Whether the user's edits ever returned to the
server was incidental: a KIO-aware handler wrote back through its own KIO
I/O, while every other handler received a KIO-managed temporary download
and its saves were lost. Remote write-in-place — open a remote file, edit
it in the desktop's normal handler, and have the saved bytes reach the
same canonical remote URL — was the remaining gap after authenticated
browse/open/rename/new-folder/copy/move, and the module must not close it
with a downloader, a sync engine, a mount framework, a polling daemon, a
private transfer protocol, or a credential authority.

The installed platform already ships the supported facility: the
`kio-fuse` daemon (D-Bus activated as `org.kde.KIOFuse`, with a systemd
user unit) exposes any KIO URL as a local FUSE path through the session-bus
`org.kde.KIOFuse.VFS.mountUrl(QString)` call, and `libKF6KIOGui`'s own
`OpenUrlJob` embeds exactly this client for its handler routing. A desktop
handler editing the local FUSE file writes ordinary local bytes; KIOFuse
owns the upload lifecycle and writes them back to the mounted URL on
close. Where the facility is absent the direct ADR-0152 open remains the
correct, already-reviewed behavior.

## Decision

The stock application injects `KioFuseRemoteFileOpener` (a
`KioRemoteFileOpener` subclass) in `main.cpp`; `NavigationController` and
its `RemoteOpenController` wiring are unchanged.

- `open()` re-validates the module's policy boundary **before any D-Bus
  contact** (supported scheme, no userinfo), so no credential material can
  leave the process through the mount call. A refused URL emits one typed
  failure, exactly like every other remote adapter.
- The canonical remote URL is passed verbatim to `mountUrl`; the returned
  absolute local path — and only an absolute path, KIOFuse being trusted
  for this mapping to the same degree as KIOGui's own kio-fuse routing —
  is opened through `KIO::OpenUrlJob` with KIO's standard UI delegate
  retained, under the same QWidget-capable application composition
  (ADR-0116/0152). Write-back belongs to KIOFuse's close lifecycle;
  QindaQt adds no downloader, mount, sync, or credential component and the
  kio-fuse daemon's lifetime belongs to the platform.
- Any facility failure — no session bus, a D-Bus error reply, or a
  malformed (empty/non-absolute) path — falls back to the base
  `KioRemoteFileOpener::open()`, i.e. byte-for-byte the pre-ADR-0152
  direct open, so remote open never regresses where KIOFuse is missing.
- Opens stay fire-and-forget with no busy state, no listing refresh, and
  no shared Cancel owner (the copy/move Cancel contract does not apply to
  open). Each `open()` is an independent operation keyed by a monotonic
  identity, so concurrent opens and late mount replies cannot alias; the
  opener's destruction kills pending watchers and jobs quietly, matching
  the ADR-0152 lifetime contract.
- `Qt6::DBus` is added to the support library as a private link for the
  mount call; no public header or ABI changes.

## Consequences

- `qindaqt.file-manager-kio-fuse-remote-opener` proves the production
  opener's scheme/userinfo refusal before contact, the
  resolve-then-open-local flow with the retained KIO UI delegate, the
  direct-open fallback on facility error/malformed reply/missing bus, the
  bounded no-credential diagnostics, independent concurrent opens, and
  quiet destruction with a pending resolution — all through the
  `createMountCall`/`canResolve`/`createOpenUrlJob` test seams, with no
  session bus, KIOFuse daemon, network, or desktop handler. Existing
  `qindaqt.file-manager-kio-remote-opener` rows keep proving the unchanged
  base opener.
- Installed/live authentication through the KIOFuse mount remains a
  separate user-controlled validation dependency; this slice claims no
  live credential or network validation.
- Batch remote writes, remote delete/Trash, mounts (S4 volumes), network
  search/preview, and a QindaQt credential-entry UI gain no authority;
  local behavior is unchanged.
- `NavigationController` stays at its 600 non-blank-line cap (ADR-0156's
  recorded decomposition decision stands); the whole behavior lives in the
  injected opener.

## Revisit when

KIO exposes a public mount/resolution API (replacing the direct D-Bus
call), kio-fuse proves unsuitable in the QindaQt session, or an explicit
user request needs write-back status/progress surfacing in File Manager
itself; any replacement goes through the same injected seam and
independent review.
