# Clipboard1 version 1

Clipboard1 is QindaQt's private, session-scoped clipboard-history protocol. Its
well-known name and interface are `org.qindaqt.Clipboard1`; its object is
`/org/qindaqt/Clipboard1`. The service is activatable, but clients must bind
all calls, replies, and signals to the current unique owner rather than trust
the well-known name as an identity.

The interface exposes metadata and user intents only. It never transports
complete clipboard payloads. Payload bytes remain in the resident service and
return to the compositor only through a successful `Copy` intent.

## Types and limits

All integers use D-Bus fixed-width types. Clipboard1 uses these structures:

| Value | Signature | Fields |
| --- | --- | --- |
| Entry id | `(uu)` | generation, serial |
| Snapshot | `(ututbbay)` | schema, epoch, generation, revision, history-enabled, privacy-allowed, QCDL descriptors |
| Operation result | `(uuttuttuts)` | kind, status, request-id, initiating epoch/generation/revision, observed epoch/generation/revision, reason-code |

Schema is exactly `1`. Epoch and generation are nonzero. The descriptor byte
array is exactly the canonical bounded `QCDL` version 1 form owned by
`clipboard_model`; every decoded entry id must carry the snapshot generation.
A snapshot whose enabled or privacy flag is false must decode to an empty
descriptor list.

Operation kind values are Select `0`, Delete `1`, Clear `2`, and Copy `3`.
Result status values are Succeeded `0`, Rejected `1`, Failed `2`, Uncertain `3`,
and Busy `4`. Reason codes contain 1–64 UTF-8 bytes and only lowercase ASCII
letters, digits, and hyphens. Unknown enum values, versions, flags, trailing
descriptor bytes, contradictory successful lineage, or malformed fields make
the whole value invalid; consumers publish no valid prefix.

## Methods

- `GetSnapshot() -> (ututbbay)` returns the current authority and metadata
  projection.
- `Select(t requestId, t epoch, u generation, t revision, (uu) entry)` promotes
  an entry without putting its payload back on Wayland.
- `Delete(...)` removes the identified entry.
- `Clear(t requestId, t epoch, u generation, t revision, b all)` removes every
  entry when `all` is true and unpinned entries otherwise.
- `Copy(...)` promotes the identified entry and publishes its bounded value as
  the regular Wayland selection.

Each mutation returns one operation-result structure. Every request id is
nonzero and scoped to the caller's D-Bus unique name. The service remembers at
most 64 results for each of at most 64 callers. A 65th simultaneous caller is
returned `Busy`/`caller-cache-full` before a caller cache is created. For an
admitted caller, a fresh request at the per-caller ceiling evicts the oldest
inserted remembered result before execution and retains the fresh result as the
newest. Repeating an identity that is still retained with identical arguments
returns its exact remembered result; reusing that retained identity with
different arguments rejects as `request-id-conflict`. Once evicted, an identity
has no exactly-once memory and is treated as a fresh request. Eviction therefore
never permits a client to assume replay safety: clients do not automatically
retry a timed-out mutation.

All intents require an exact expected epoch, generation, and revision. A stale
or restarted view rejects without mutation. A successful result preserves the
initiating epoch and generation and reports an observed revision no lower than
the initiating revision.

## Signal and client behavior

`Changed(t epoch, u generation, t revision)` invalidates the cached snapshot.
It is not a delta and grants no mutation authority. Clients coalesce signals
and fetch a complete snapshot from the same exact owner. Owner replacement,
owner loss, transport errors, malformed values, lineage regression, or a late
reply withdraw cached authority. At most one client mutation is in flight;
timeouts complete as uncertain and are never replayed.

## Privacy behavior

The resident host enables capture only after Settings1 has confirmed the
`services.clipboardHistory` opt-in and authenticated session-lock state is
conclusively unlocked. Disabling the setting or losing unlocked truth clears
history, advances generation, publishes an empty snapshot, and stops capture.
Unknown or unavailable authority is equivalent to denied authority.

See [Clipboard service](../architecture/clipboard-service.md) and
[ADR-0058](../adr/0058-isolate-clipboard-capture-in-a-volatile-host.md).
