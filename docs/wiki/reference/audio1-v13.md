# Audio1 schema 13: per-device latency offsets

This page describes the schema-13 additions to the
[Audio1 version-2 base contract](audio1-v2.md) and
[schema 12](audio1-v12.md). Everything else is unchanged. The decision is
[ADR-0288](../adr/0288-per-device-latency-offsets-and-a-compact-audio-page.md).

`Snapshot.schemaVersion` is 13. An older client rejects the snapshot and an
older service has no `SetLatencyOffset` method, so mixed versions fail closed.

## Device fields

`Device` appends five fields after `nodeName`; its D-Bus tuple is
`((tt)ussdbbbbbbadasbsbxbxx)`.

| Field | D-Bus | Meaning |
| --- | --- | --- |
| `latencyOffsetKnown` | `b` | The node reports a PipeWire `latencyOffsetNsec` inside the Audio1 window |
| `latencyOffsetNs` | `x` | The reported offset in signed nanoseconds; `0` when unknown |
| `canSetLatencyOffset` | `b` | Settable: known, capability present, a declared range, and a `node.name` |
| `latencyOffsetMinNs` | `x` | Lowest settable offset; `0` unless settable |
| `latencyOffsetMaxNs` | `x` | Highest settable offset; `0` unless settable |

The Audio1 window is −2 s to +2 s. The settable range is the node's own
`PropInfo` range clipped to that window: ALSA declares `0..2 s`, Bluetooth
sinks the whole signed range. A reading outside the window is published as
unknown, never clamped. Validation rejects any non-canonical combination: an
unknown offset with a value or range, a settable one without bit 11, a range
out of order or outside the window, or a value outside its settable range.
Surfaces must show an unknown offset as absent, not as 0 ms.

## Operation

`SetLatencyOffset` is capability bit 11 and operation kind 30:

```text
SetLatencyOffset((tt) device, x offsetNs) -> (uuttttss)
```

The device must be a current output or input with `canSetLatencyOffset`, and
the value inside its settable range. Audio1 remembers the value by the
device's `node.name` in `$XDG_CONFIG_HOME/qindaqt/audio-latency.json`
(`QINDAQT_AUDIO_LATENCY_PATH` overrides it for a probe) and completes
immediately: `Succeeded` means remembered and declared. The applied value is
confirmed only by `latencyOffsetNs` in a later snapshot. Refusals use the
ordinary result: `Unsupported/unsupported`, `Rejected/stale-handle`,
`Rejected/invalid-latency-offset`, `Rejected/too-many-latency-offsets`, and
`Failed/latency-store-unwritable`.

Audio1 writes a remembered offset to the device node each time the device
appears, answers a device that resets it with at most three writes per
declaration, and never writes a device it has no offset remembered for. The
offset changes the latency the device reports, which players use for A/V sync;
it does not add delay to console routing.
