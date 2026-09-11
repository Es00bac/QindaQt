# Audio1 protocol version 2

Audio1 is the bounded control and observation interface exported by
`qindaqt-audio-service`. Version 2 adds per-channel truth and managed virtual
devices ([ADR-0123](../adr/0123-voicemeeter-class-audio-graph-on-pipewire.md),
slice S1). Version 1 remains documented as the
[historical contract](audio1-v1.md).

| Property | Value |
| --- | --- |
| Bus name | `org.qindaqt.Audio1` |
| Object path | `/org/qindaqt/Audio1` |
| Interface | `org.qindaqt.Audio1` |
| Snapshot schema | `2` |

The canonical machine-readable interface is installed as
`org.qindaqt.Audio1.xml`.

## Scalar meanings

All `t` values are unsigned 64-bit integers; `u` values are unsigned 32-bit
integers. Levels are finite doubles in the inclusive range `[0.0, 1.0]`.

`Availability` values are `Starting=0`, `Ready=1`, `Unavailable=2`, and
`Degraded=3`. Capability bits are `SetDefault=1`, `SetVolume=2`, `SetMute=4`,
`MoveStream=8`, `SetChannelVolumes=16`, and `ManageVirtualDevices=32`.
Device kinds are `Output=0` and `Input=1`; stream directions
are `Playback=0` and `Capture=1`.

An `OperationResult` kind is `SetDefault=0`, `SetVolume=1`, `SetMute=2`,
`MoveStream=3`, `SetChannelVolumes=4`, `CreateVirtualDevice=5`, or
`RemoveVirtualDevice=6`. Status is `Succeeded=0`, `Rejected=1`,
`Unsupported=2`, `Failed=3`, `Uncertain=4`, or `Busy=5`.

## Fixed structures

No Audio1 domain value is an `a{sv}` property bag.

| Value | Signature | Fields in order |
| --- | --- | --- |
| Handle | `(tt)` | epoch, PipeWire `object.serial` |
| Device | `((tt)ussdbbbbbbadasb)` | handle, kind, name, description, volume, volume-known, muted, mute-known, default, can-set-volume, can-set-mute, channel volumes, channel map, virtual device |
| Stream | `((tt)uss(tt)bdbbbbbbadas)` | handle, direction, application name, media name, target, target-known, volume, volume-known, muted, mute-known, can-set-volume, can-set-mute, can-move, channel volumes, channel map |
| Operation result | `(uuttttss)` | kind, status, initiating epoch/revision, observed epoch/revision, reason code, diagnostic |

The snapshot signature is:

```text
(uttuuss(tt)(tt)a((tt)ussdbbbbbbadasb)a((tt)ussdbbbbbbadasb)a((tt)uss(tt)bdbbbbbbadas))
```

Its fields are schema version, epoch, revision, availability, capabilities,
reason code, diagnostic, default output, default input, outputs, inputs, and
streams.

An unknown default or stream target is the zero handle `(0,0)` and has its
corresponding known/default evidence cleared. Every nonzero handle contained in
a snapshot has exactly the snapshot epoch. Serial values are unique across the
retained devices and streams. Arrays are ascending by serial. A known playback
target names a retained output; a known capture target names a retained input.

## Channel truth

`channelMap` is the node's negotiated position layout as SPA position labels
(`FL`, `FR`, `FC`, `LFE`, `SL`, `SR`, `RL`, `RR`, `AUX0`, …), bounded to 32
labels of at most 24 UTF-8 bytes each. It is read from the node's
`audio.position`; a node without positions publishes an empty map.

`channelVolumes` is the mixer's per-channel state projected onto that layout:
when the map is non-empty, exactly one volume is published per mapped channel,
and channels the mixer has not yet reported carry the aggregate level. A
stream or device whose mixer state is unknown publishes an empty
`channelVolumes`. When both arrays are non-empty they must describe the same
channel count; a mismatch fails closed.

The first per-channel write on a freshly observed multi-channel device can make
the mixer adopt the full negotiated layout (PipeWire re-sizes the node's
channel volume array when its props change); until then, values read back as
one uniform level. `SetChannelVolumes` is therefore validated against the
retained snapshot's channel count and never writes a partial layout.

`virtualDevice` is true only for managed null devices created through
`CreateVirtualDevice` (or re-adopted after a service restart because they
linger). Provenance is the `qindaqt.virtual.` node-name prefix. A sink's
monitor source enumerates like any other input and is not a managed virtual
device.

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

`SetChannelVolumes(Handle target, ad volumes) -> OperationResult` sets one
volume per channel of the target's retained layout. The request is rejected
with `invalid-target` unless the count matches the retained snapshot exactly,
and with `invalid-volume` when any level is nonfinite or outside `[0,0,1]`.

`CreateVirtualDevice(u kind, s displayName, u channels) -> OperationResult`
creates a managed null device (`support.null-audio-sink` through the `adapter`
factory) with `object.linger=true`, the requested 2, 4, 6, or 8 channel
position layout, `node.name = qindaqt.virtual.<slug>` derived from the display
name with a numeric suffix on collision, and `media.class` `Audio/Sink` or
`Audio/Source` per kind. The device persists across service restarts and is
re-adopted into later snapshots with `virtualDevice=true`.

`RemoveVirtualDevice(Handle device) -> OperationResult` destroys a managed
virtual device. It rejects any device without the managed provenance — a
hardware node can never be destroyed through Audio1 — and the backend enforces
the same fence against the node name.

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
| Channels per device or stream | 32 |
| Channel position label | 24 UTF-8 bytes |
| Virtual device display name | 96 UTF-8 bytes |
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
`invalid-volume`, `invalid-target`, `invalid-name`, `invalid-channel-count`,
`incompatible-target`, `unsupported`, `too-many-operations`,
`operation-timeout`, `owner-replaced`, `authority-replaced`,
`client-stopped`, `wireplumber-replaced`, `pipewire-replaced`,
`malformed-snapshot`, `malformed-result`, and `backend-malformed`. Callers
must branch on status and reason code rather than diagnostic text.

## Compatibility

Schema version 2 has fixed structures and enum values. New optional behavior
is advertised with capability bits and existing known/can booleans. Adding
fields, changing signatures, changing enum meanings, or weakening validation
requires a new interface/schema version. Unknown versions, enum values,
capability bits, malformed lineages, out-of-order/duplicate serials, invalid
target kinds, channel layouts whose volumes and map disagree,
nonfinite/out-of-range known levels, and oversized payloads fail closed.

See the [Audio service architecture](../architecture/audio-service.md) for
thread ownership, backend behavior, activation, and qualification scope.
