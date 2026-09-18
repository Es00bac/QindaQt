# ADR-0195: One owner per transfer, and a queue for the ones that cross the network boundary

- **Status:** Proposed
- **Date:** 2026-09-17
- **Owners:** File Manager
- **Supersedes:** None
- **Superseded by:** None

## Context

Copy To and Move To had two owners and a gap between them.
`MutationController` owned local copies and moves, with identity checks,
cross-device refusal, Trash, and undo. ADR-0155 and ADR-0156 added a
deliberately narrow remote path: **one** listed child of the folder being
browsed, to a destination folder on the **same** authority, one at a time.
Between them there was nothing, so the thing a user actually wants from a
network file manager — dragging a file from the desktop onto a share, or
pulling one back — could not be expressed at all. A remote multi-selection
was refused before the destination dialog could even open.

Which owner runs a request was decided in QML, by asking
`navigationController.remoteActive`. That question is about the folder being
browsed, not about the pair of endpoints, so it cannot distinguish "remote
folder, local destination" from "remote folder, remote destination" — and it
was the reason the one-child contract had to be enforced twice, once before
the dialog opened and once at accept time.

## Decision

**The owner of a Copy To / Move To request is decided once, in C++, from the
exact pair of source list and destination**, by `TransferRouter::route()`.
It is pure policy: no I/O, no stat, no symlink resolution, no server contact.
It names one of four owners:

| Route | Owner | When |
| --- | --- | --- |
| `local` | `MutationController` | no endpoint is on the network |
| `remote-child` | the ADR-0155/0156 path | exactly one source, both endpoints on one network authority |
| `queue` | `TransferQueueController` | anything else involving a network endpoint |
| `refuse` | nobody | an unusable endpoint, or a destination inside its own source |

The router can never name `remote-child` for more than one source, which is
what lets the destination dialog open on a remote multi-selection: the
one-child contract is now enforced where the dispatch happens instead of by
refusing to open a dialog.

`TransferQueueController` owns order, dispatch, and every piece of visible
state for network transfers:

- One source becomes one item, so a ten-file selection is ten items and a
  single failure retires only its own item.
- **Exactly one item runs at a time.** A saturated link makes concurrent
  transfers slower rather than faster, and one running job keeps the
  platform's credential prompts sequential.
- Pause, resume, cancel, and cancel-all are the user's; the queue never
  retries on its own. Cancel-all retires every live item *before* dispatching
  anything, so it cannot hand the platform work the user just asked to stop.
- An item that is not Queued, Running, or Paused has been retired. A later
  worker result for such an item is dropped. This fence is what makes cancel
  reliable: a quiet KIO kill deliberately delivers no result at all.
- A confirmed success emits `transferCommitted(destinationFolder)`. The
  window re-reads the listing only when that is the folder on screen; the
  queue never navigates or refreshes.

`TransferWorker` is the injected platform seam. `KioTransferWorker` implements
it on `KIO::copy()`/`KIO::move()` and re-proves the boundary itself, ahead of
the router: each endpoint must be a canonical `smb`/`sftp` URL or an absolute
local `file://` URL, neither may carry userinfo, and **at least one must be on
the network** — a purely local transfer must never reach KIO from here,
because `MutationController` has identity checks this worker has not.

Overwrite and conflict resolution stay with KIO's standard UI delegate
(ADR-0151). Nothing in the queue silently overwrites; a QindaQt-owned
conflict dialog is a later slice.

## Consequences

- Copying and moving both ways between the desktop and the laptop works from
  the ordinary Copy To / Move To actions, with a banner showing what is
  running, how far along, and how many are waiting.
- The ADR-0155/0156 path is untouched and still owns exactly the case it was
  reviewed for, including its destructive-move guards.
- Two collaborators can still run at once (a one-child remote copy and a
  queued transfer), which is why Cancel routes to whichever is busy.
- Drag-and-drop across the boundary is **not** part of this decision: the
  drop pipeline is the identity-checked local mutation one and has no network
  authority. It stays local-only.
- Focused rows: `qindaqt.file-manager-transfer-router`,
  `qindaqt.file-manager-transfer-queue` (against a recording worker double —
  no KIO, no network), `qindaqt.file-manager-kio-transfer-worker`.

## Revisit when

Per-item progress proves too coarse for large trees, a user asks for more
than one concurrent transfer on a fast link, or conflict resolution needs to
move from KIO's dialog into a QindaQt one — the last of these will want the
queue to carry a per-item conflict state rather than a new owner.
