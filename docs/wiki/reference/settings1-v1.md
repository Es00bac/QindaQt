# Settings1 protocol version 1

## Endpoint and trust scope

- Well-known name/interface: `org.qindaqt.Settings1`
- Object: `/org/qindaqt/Settings1`
- Authority: ordinary same-user session service; writes affect user overrides
- Lineage: exact unique owner plus fresh service epoch; no PID/executable proof

The wire schema version is independent of persisted settings schema v2.

## Methods and signal

`GetSnapshot(keys)` returns status, exact wire-schema version, active persisted
settings-schema version, epoch, revision, effective values, source layers, and
a bounded message. The request is rejected as a whole when any key is unknown,
duplicated, or malformed. A client rejects a missing or non-v1 wire version;
the reported settings-schema version is lineage metadata rather than the wire
compatibility version.

`CommitUserTransaction(epoch, baseRevision, operations)` accepts a bounded list
of unique-key `set`/`remove` operations. It returns a typed status, revision
before/after, authoritative current value/source for every operated key,
changed keys, and a bounded message. Statuses distinguish applied, validation,
conflict, read-only layer, persistence failure, revision exhaustion, epoch
mismatch, unknown key, and malformed request. Applied no-ops keep revision and
publish nothing.

`SettingsChanged(epoch, revision, changedKeys)` is an invalidation hint. A
client subscribed to the exact unique sender fetches a complete scoped
snapshot; it does not assemble authority from signals.

## Value and resource bounds

Values are JSON-native null, Boolean, bounded integer/finite number, UTF-8
string, array, or string-keyed object. Arrays and objects may nest. The v1
limits are:

| Resource | Limit |
| --- | ---: |
| Requested keys / transaction operations / changed keys | 64 |
| Key UTF-8 bytes | 256 |
| String UTF-8 bytes | 16,384 |
| Entries per list / map | 512 / 256 |
| Value depth / nodes | 16 / 4,096 |
| Aggregate nodes per snapshot / transaction | 16,384 / 16,384 |
| Aggregate bytes per value / snapshot / transaction | 262,144 / 1,048,576 / 1,048,576 |

Duplicate operations, non-finite numbers, opaque Qt values, malformed D-Bus
containers, or any bound overflow reject the complete request before it reaches
the settings model.
