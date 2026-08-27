# Audio1 protocol version 1

Audio1 is the bounded control and observation interface exported by
`qindaqt-audio-service`.

| Property | Value |
| --- | --- |
| Bus name | `org.qindaqt.Audio1` |
| Object path | `/org/qindaqt/Audio1` |
| Interface | `org.qindaqt.Audio1` |
| Snapshot schema | `1` |

The canonical machine-readable interface is installed as
`org.qindaqt.Audio1.xml`.

## Scalar meanings

All `t` values are unsigned 64-bit integers; `u` values are unsigned 32-bit
integers. Levels are finite doubles in the inclusive range `[0.0, 1.0]`.

`Availability` values are `Starting=0`, `Ready=1`, `Unavailable=2`, and
`Degraded=3`. Capability bits are `SetDefault=1`, `SetVolume=2`, `SetMute=4`,
and `MoveStream=8`. Device kinds are `Output=0` and `Input=1`; stream directions
are `Playback=0` and `Capture=1`.

An `OperationResult` kind is `SetDefault=0`, `SetVolume=1`, `SetMute=2`, or
`MoveStream=3`. Status is `Succeeded=0`, `Rejected=1`, `Unsupported=2`,
`Failed=3`, `Uncertain=4`, or `Busy=5`.

## Fixed structures

No Audio1 domain value is an `a{sv}` property bag.

| Value | Signature | Fields in order |
| --- | --- | --- |
| Handle | `(tt)` | epoch, PipeWire `object.serial` |
| Device | `((tt)ussdbbbbbb)` | handle, kind, name, description, volume, volume-known, muted, mute-known, default, can-set-volume, can-set-mute |
| Stream | `((tt)uss(tt)bdbbbbbb)` | handle, direction, application name, media name, target, target-known, volume, volume-known, muted, mute-known, can-set-volume, can-set-mute, can-move |
| Operation result | `(uuttttss)` | kind, status, initiating epoch/revision, observed epoch/revision, reason code, diagnostic |

The snapshot signature is:

```text
(uttuuss(tt)(tt)a((tt)ussdbbbbbb)a((tt)ussdbbbbbb)a((tt)uss(tt)bdbbbbbb))
```

Its fields are schema version, epoch, revision, availability, capabilities,
reason code, diagnostic, default output, default input, outputs, inputs, and
streams.

An unknown default or stream target is the zero handle `(0,0)` and has its
corresponding known/default evidence cleared. Every nonzero handle contained in
a snapshot has exactly the snapshot epoch. Serial values are unique across the
retained devices and streams. Arrays are ascending by serial. A known playback
target names a retained output; a known capture target names a retained input.

## Methods and signal

`GetSnapshot() -> Snapshot` returns the current complete bounded snapshot.

`SetDefault(Handle device) -> OperationResult` selects an output or input as
the configured default according to its typed device kind.

`SetVolume(Handle target, double volume) -> OperationResult` sets one device or
application stream to a normalized level.

`SetMute(Handle target, bool muted) -> OperationResult` changes device or
application-stream mute state.

`MoveStream(Handle stream, Handle device) -> OperationResult` moves playback
only to an output and capture only to an input.

`Changed(t epoch, t revision)` is an invalidation hint. Receivers fetch a full
snapshot and never treat the signal as data. Public clients bind both calls and
signals to the exact unique owner resolved for the well-known name.

## Limits

| Field | Maximum |
| --- | ---: |
| Outputs | 128 |
| Inputs | 128 |
| Streams | 256 |
| Display name/description/media name | 256 UTF-8 bytes each |
| Application name | 256 UTF-8 bytes |
| Reason code | 64 UTF-8 bytes |
| Diagnostic | 512 UTF-8 bytes |
| Service operations in flight | 64 |

The decoder consumes an entire oversized array but marks the value invalid and
retains at most the limit. Publishers reject it atomically. Text may not contain
NUL. Diagnostics replace unsafe control characters and truncate on a UTF-8
boundary. The service never exports raw WirePlumber properties or environment,
filesystem, device, or sample data.

## Lineage and completion

Epoch identifies one service/WirePlumber/PipeWire authority lineage. A service
unique-owner change, WirePlumber daemon replacement, or PipeWire reconnect
invalidates earlier handles. Revision is monotonic snapshot publication within
an epoch.

An operation result records both initiating and observed lineage. `Succeeded`
is valid only in the initiating epoch and at an observed revision no earlier
than initiation. Once a mutation has been dispatched, timeout or authority loss
returns `Uncertain`; clients refetch but never replay it. A late result for an
old owner, request ID, epoch, revision, or operation kind is ignored or treated
as malformed.

The public Qt client returns a nonzero request ID before it emits that request's
completion. Local rejection, busy/unsupported classification, transport reply,
timeout, and uncertainty all use the same queued exactly-once completion path.
Explicit stop cancels an undelivered local/accepted result, while a mutation
still awaiting its transport result completes asynchronously as
`Uncertain/client-stopped`; client destruction drops queued delivery safely.

Common stable reason codes include `unavailable`, `stale-handle`,
`invalid-volume`, `incompatible-target`, `unsupported`,
`too-many-operations`, `operation-timeout`, `owner-replaced`,
`authority-replaced`, `client-stopped`, `wireplumber-replaced`,
`pipewire-replaced`, `malformed-snapshot`, `malformed-result`, and
`backend-malformed`. Callers must branch on status and reason code rather than
diagnostic text.

## Compatibility

Schema version 1 has fixed structures and enum values. New optional behavior is
advertised with capability bits and existing known/can booleans. Adding fields,
changing signatures, changing enum meanings, or weakening validation requires a
new interface/schema version. Unknown versions, enum values, capability bits,
malformed lineages, out-of-order/duplicate serials, invalid target kinds,
nonfinite/out-of-range known levels, and oversized payloads fail closed.

See the [Audio service architecture](../architecture/audio-service.md) for
thread ownership, backend behavior, activation, and qualification scope.
