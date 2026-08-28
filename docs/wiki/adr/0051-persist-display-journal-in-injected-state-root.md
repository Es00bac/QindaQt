# ADR-0051: Persist Display1 journal in an injected state root

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Display platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0016](0016-display1-transaction-authority.md) makes the Display1 journal a
hard gate before a preview may mutate compositor state. The D1 model and codec
are pure, and the D4 writer intentionally owns no filesystem policy. A service
crash between preview and confirmation must leave one complete pre-image for a
later resident process, while a torn, hostile, or redirected file must never be
interpreted as recovery authority.

Looking up `HOME` or an XDG variable inside a low-level store would hide
authority and make private-session tests host-dependent. A delete-then-rename
sequence would introduce a window with no recovery journal. Following a
symlink or accepting a group-writable state root would let an unintended path
become transaction authority. The owning architecture is described in
[Display service](../architecture/display-service.md).

## Decision

`display_journal` is a separate synchronous filesystem adapter. Its constructor
receives one existing QindaQt user-state directory; the module never discovers,
creates, or expands a home/XDG path. The directory must be a non-symlink,
effective-user-owned directory that is not group- or other-writable. The
adapter can address only the fixed `display1-transaction.journal` name and its
fixed same-directory temporary name.

Stores encode through the accepted D1 canonical versioned codec, create the
temporary file exclusively with mode `0600`, write the complete payload, sync
the file, atomically rename it over the prior regular single-link journal, and
sync the containing directory where the filesystem supports that operation.
Any failure before rename removes only the adapter's temporary name and leaves
the prior committed journal unchanged. A stale non-directory temporary name is
safe to unlink on the next store; a directory collision fails closed.

Loads return exactly one of absent, loaded, or rejected. They inspect without
following links, require an effective-user-owned regular single-link file with
no group/other permissions, enforce the D1 byte ceiling before and during the
read, decode to a temporary value, and require canonical re-encoding to match
the stored bytes. A malformed or unsafe journal is retained and reported as
rejected; it is never silently cleared or quarantined. A stale temporary file
is ignored, so interruption before rename exposes the preceding committed
journal or absence. Clear validates the final path, unlinks only that regular
file, and applies the directory durability barrier.

The resident composition remains the single writer. `load()` supplies the
deterministic startup seam: the future process passes a loaded value to D1
`Machine::recover` and retains the file until the model proves that clearing is
safe. This outcome does not wire the D4 writer into the packaged process or
claim nested compositor recovery.

## Consequences

- Filesystem authority is visible at construction and bounded to one supplied
  directory plus two fixed names; tests need no host environment or session.
- Atomic replacement preserves a complete prior or new canonical journal
  across ordinary process interruption. Supported Linux filesystems also
  receive file and directory durability barriers.
- Symlinks, directories, devices, FIFOs, hard links, wrong ownership, loose
  permissions, oversize content, and malformed/non-canonical bytes fail closed.
- The caller must create/select the QindaQt state root and must serialize
  resident store/load/clear operations. Lock/session policy, startup
  composition, and compositor convergence remain separate boundaries.
- Exceptional storage hardware errors reported after the atomic commit point
  remain platform I/O failures; the service must not claim preview safety when
  `store()` returns false.

## Revisit when

Revisit if Linux provides a stronger broadly available atomic durable-replace
primitive, if multiple cooperating Display1 writers become a requirement, or
if the canonical journal schema needs migration rather than fail-closed version
rejection. Convenience path discovery alone is not sufficient.
