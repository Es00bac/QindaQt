# Bluetooth1 protocol version 1

Bluetooth1 is the bounded control and observation interface exported by
`qindaqt-bluetooth-service`.

| Property | Value |
| --- | --- |
| Bus name | `org.qindaqt.Bluetooth1` |
| Object path | `/org/qindaqt/Bluetooth1` |
| Interface | `org.qindaqt.Bluetooth1` |
| Snapshot schema | `1` |

The canonical machine-readable interface is installed as
`org.qindaqt.Bluetooth1.xml`.

## Scalar meanings

All `t` values are unsigned 64-bit integers; `u` values are unsigned 32-bit
integers; `s` is UTF-8 string; `b` is boolean; `n` is signed 16-bit integer.

`AdapterState` values are `Off=0` and `On=1`. `AdapterCapability` bits are
`Discover=1`, `Pair=2`, and `Connect=4`. Device states are `Disconnected=0`,
`Connecting=1`, and `Connected=2`. Device capability bits are `Pair=1`,
`Connect=2`, `Disconnect=4`, and `Trust=8`. Operation kinds are `Pair=0`,
`Connect=1`, `Disconnect=2`, `Trust=3`, and `Untrust=4`. Operation status is
`Succeeded=0`, `Rejected=1`, `Unsupported=2`, `Failed=3`, `Uncertain=4`, or
`Busy=5`.

An `OperationResult` kind uses the same enumeration as `OperationRequest`.

## Fixed structures

No Bluetooth1 domain value is an `a{sv}` property bag.

| Value | Signature | Fields in order |
| --- | --- | --- |
| Handle | `(tt)` | epoch, object serial |
| Adapter | `((tt)ssunnb)` | handle, address, name, state, capabilities, discovering |
| Device | `((tt)(tt)ssnnbbbbbuu)` | handle, adapter handle, address, name, state, RSSI, RSSI known, paired, trusted, capabilities |
| Operation result | `(uuttttss)` | kind, status, initiating epoch, initiating revision, observed epoch, observed revision, reason code, diagnostic |

The snapshot signature is:

```text
(uttssbba((tt)ssunnb)a((tt)(tt)ssnnbbbbbuu))
```

Its fields are schema version, epoch, revision, reason code, diagnostic, adapters,
and devices.

An adapter address is a Bluetooth MAC address in canonical form `XX:XX:XX:XX:XX:XX`.
A device's adapter handle must reference a valid adapter in the same snapshot.
Arrays are required to be in ascending serial order. RSSI is -127..127 or 0 when
unknown.

## Methods and signal

`GetSnapshot() -> Snapshot` returns the current complete bounded snapshot.

`Pair(Handle device) -> OperationResult` initiates pairing with a device.

`Connect(Handle device) -> OperationResult` connects to a paired device.

`Disconnect(Handle device) -> OperationResult` disconnects from a device.

`Trust(Handle device) -> OperationResult` marks a device as trusted.

`Untrust(Handle device) -> OperationResult` removes trust from a device.

`Changed(t epoch, t revision)` is an invalidation hint. Receivers fetch a full
snapshot and never treat the signal as data. Public clients bind both calls and
signals to the exact unique owner resolved for the well-known name.

## Limits

| Field | Maximum |
| --- | ---: |
| Adapters | 16 |
| Devices | 256 |
| Display address | 32 UTF-8 bytes |
| Display name | 256 UTF-8 bytes |
| Reason code | 64 UTF-8 bytes |
| Diagnostic | 512 UTF-8 bytes |
| Service operations in flight | 64 |

The decoder consumes an entire oversized array but marks the value invalid and
retains at most the limit. Publishers reject it atomically. Text may not contain
NUL. Diagnostics replace unsafe control characters and truncate on a UTF-8
boundary. The service never exports raw BlueZ properties or environment, filesystem,
device data, or secrets.

## Lineage and completion

Epoch identifies one service authority lineage. A service unique-owner change or
process restart invalidates earlier handles. Revision is monotonic snapshot
publication within an epoch.

An operation result records both initiating and observed lineage. `Succeeded` is
valid only in the initiating epoch and at an observed revision no earlier than
initiation. Once a mutation has been dispatched, service-owner loss returns
`Uncertain`; clients refetch but never replay it. A late result for an old owner,
request ID, epoch, revision, or operation kind is ignored or treated as malformed.

The public Qt client returns a nonzero request ID before it emits that request's
completion. Local rejection, busy/unsupported classification, transport reply,
timeout, and uncertainty all use the same queued exactly-once completion path.

Common stable reason codes include `unavailable`, `stale-handle`, `device-not-found`,
`not-paired`, `already-paired`, `already-connected`, `already-disconnected`,
`already-trusted`, `already-untrusted`, `unsupported`, `too-many-operations`,
`operation-timeout`, `owner-replaced`, `client-stopped`, and `backend-malformed`.
Callers must branch on status and reason code rather than diagnostic text.

## Compatibility

Schema version 1 has fixed structures and enum values. New optional behavior is
advertised with capability bits. Adding fields, changing signatures, changing enum
meanings, or weakening validation requires a new interface/schema version. Unknown
versions, enum values, capability bits, malformed lineages, out-of-order/duplicate
serials, nonfinite/out-of-range RSSI, and oversized payloads fail closed.

See the [Bluetooth service architecture](../architecture/bluetooth-service.md) for
design principles, handle lineage, backend behavior, activation, and qualification
scope.
