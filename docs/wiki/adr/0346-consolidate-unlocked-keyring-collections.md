# ADR-0346: Consolidate unlocked keyring collections inside the resident

- Status: Accepted
- Date: 2026-10-03
- Owners: QindaQt program manager and keyring module
- Scope: Native Secret Service persistence and collection identity

## Context

The one-time import preserved separate encrypted collections. ADR-0343 lets
one authenticated password unlock matching stores, but a user who wants one
physical wallet still has several files and several collection paths. A
client-side move would expose item secrets to ordinary client memory and could
race another writer between copy verification and source deletion. The
resident already owns the sole-writer catalog lease and protected item pages.

## Decision

`org.qindaqt.Keyring1.ConsolidateCollections(target, sources) -> savedItems`
accepts exact persistent collection paths, all unlocked. It rejects duplicate
or aliased sources, the volatile collection, and conflicting item IDs. Within
the resident event loop, the repository copies item IDs, metadata, attributes
and secrets into move-only secure pages, then makes one durable encrypted batch
save to the target. Only after that save succeeds does it remove each source
from the catalog and retire its encrypted file. It reports success and emits
Secret Service changes after those commits. An interrupted deletion may leave
both the saved target copies and a source; an exact retry recognizes identical
items and completes retirement without duplicating them. Uncertain durability
retires the broker before another mutation.

The operation preserves the target password and default alias. Source object
paths cease to exist after success; clients must rediscover moved items by
attributes. An operator takes and verifies an encrypted backup before a live
consolidation. No password or item value enters the method arguments, logs,
temporary files or a client-side migration process.

## Consequences

The service can meet a literal one-store request without turning a login
unlock into an implicit migration. Private-bus fixtures must prove copy,
source retirement, restart persistence and locked-source refusal. A later
Settings control can use this native method, but it must present the path
change and backup policy to its caller. The method is limited to same-UID
callers by the existing resident admission boundary.

See [native keyring daemon](../architecture/keyring-daemon.md) and
[storage](../architecture/keyring-storage.md).
