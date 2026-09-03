# ADR-0064: Confine File Manager mutation to identity-checked local authority

- **Status:** Accepted
- **Date:** 2026-09-03
- **Owners:** File Manager
- **Supersedes:** None
- **Superseded by:** None

## Context

File Manager S0 deliberately exposed read-only local navigation plus the
bounded launch intent in [ADR-0029](0029-file-manager-bounded-local-launch.md).
S1 must add useful local mutation and recoverable deletion without turning the
application into a mount broker, portal, shell service, or unbounded filesystem
worker. Paths shown by a directory listing can also vanish, change identity, or
be replaced by symbolic links before a user confirms an operation.

The Text Editor already establishes the relevant durability convention in
[ADR-0022](0022-keep-text-documents-local-and-atomic.md): local writes carry an
optimistic identity precondition and refuse conflict rather than overwriting
changed state. Trash additionally has an interoperability format defined by
the [freedesktop.org Trash specification](https://specifications.freedesktop.org/trash/latest/).

## Decision

File Manager owns one private, typed mutation boundary with these rules:

- A GUI-thread controller accepts only create-folder, rename, copy, move,
  trash, restore, empty-trash, undo, and cancellation intents. Exactly one
  request runs at a time on an owned worker thread; progress and terminal
  results return to the GUI thread as bounded values.
- Every source and parent precondition carries the listing-time POSIX identity
  (device, inode, size, modification time, and mode). A missing or different
  identity is `vanished` or `changed`; the operation never guesses or
  overwrites. Identity fields cross the QML seam as decimal strings so
  JavaScript cannot round 64-bit values. Existing destinations are always
  refused at commit time with `renameat2(RENAME_NOREPLACE)`; the preflight
  existence check is not overwrite authority.
- Requests declare absolute local roots. The backend lexically contains each
  path in those roots and checks every observed path component with `lstat`;
  symbolic links are not traversed by mutation or recursive deletion. Recursive
  copy and cleanup additionally pin every directory with descriptor-relative
  `openat`/`fstatat` operations and `O_NOFOLLOW`, then recheck each visited
  identity. Copy is capped at 20,000 entries and cleans its partial destination
  on cancellation, hostile content, or source-identity change.
- Rename and move use same-filesystem rename semantics. Copy preserves mode and
  modification time where Qt and the platform permit. Cross-device move is a
  typed refusal rather than an implicit copy/delete operation. An unchanged
  rename name is a controller-level no-op.
- Recoverable deletion uses only the home Trash at
  `$XDG_DATA_HOME/Trash/{info,files}`. A mode-0600, exclusively created
  `.trashinfo` record containing the percent-encoded absolute path and local
  deletion time is flushed before its same-named payload is renamed into
  `files/`. Name collisions in either metadata or payload storage receive
  bounded suffixes, including orphan payloads. If source and home Trash
  devices differ, the source is retained and `cross-device` is returned;
  per-volume Trash is deliberately not claimed.
- Restore accepts only the controller-retained opaque Trash token and identity,
  revalidates the `.trashinfo` destination against its declared root, and
  refuses a collision or device change. An unresolved or vanished restore
  parent is `vanished`, not `cross-device`. Empty Trash is separately confirmed,
  cancellable between entries, and never removes the `files/` or `info/`
  directories themselves.
- One-level undo is retained only for successful create, rename, and move.
  Trash has a separate one-level restore affordance. The AppShell action
  catalog is the sole keyboard/menu dispatch surface and marks Trash and Empty
  Trash destructive so presentation must confirm them.

The module remains application-private and has no stable library ABI. Typed
operation/error values and injected backend/device seams are its testable
boundary; QML has no filesystem authority.

## Consequences

- Local mutation cannot block the GUI event loop, silently overwrite a changed
  selection, follow an observed symlink escape, or erase a cross-volume source
  under the pretense that home Trash accepted it.
- Home-volume Trash entries interoperate with other implementations of the
  freedesktop.org format. Per-volume Trash, mounts, portals, search, previews,
  and network locations remain separate outcomes.
- Copy and empty-trash cancellation are cooperative at item boundaries. A
  single filesystem syscall already in progress is not preempted.
- Tests must inject filesystem/device and backend seams, use fixture-local
  Trash roots, unset ambient desktop/bus variables for UI probes, and include
  changed/vanished, permission, collision, cross-device, hostile-name,
  symlink-poison, orphan-payload, and racing-writer controls. A production-QML
  row must drive identity-carrying actions through AppShell and the real dialogs.

## Revisit when

Per-volume Trash can be implemented and qualified without mount authority,
descriptor-relative traversal becomes a shared cross-application boundary, or
multi-operation recovery needs a durable journal rather than one-level
process-local state.
