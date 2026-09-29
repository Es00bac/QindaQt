# Audio1 schema 14: a real per-bus delay stage

This page describes the schema-14 addition to the
[Audio1 version-2 base contract](audio1-v2.md) and
[schema 13](audio1-v13.md). Everything else is unchanged. The decision is the
[2026-09-28 addendum to ADR-0288](../adr/0288-per-device-latency-offsets-and-a-compact-audio-page.md#addendum-2026-09-28-a-real-per-bus-delay-stage).

`Snapshot.schemaVersion` is 14. An older client rejects the snapshot, as with
every earlier schema step.

## BusProcessing field

`BusProcessing` (the bus rack, ADR-0180, carried in every `Console.buses[].processing`)
appends one field after `mode`, its previous last field; its D-Bus tuple is
`((bddddddd)ui)`: the nested equalizer tuple, `mode` (uint32), then the new
field.

| Field | D-Bus | Meaning |
| --- | --- | --- |
| `delayMs` | `i` | Real delay in the graph, whole milliseconds, 0..1000, default 0 |

There is no per-device equivalent to `latencyOffsetKnown`/`canSetLatencyOffset`
here: unlike a device's latency offset, every bus always has a settable
`delayMs`. `validBusProcessing` rejects a value outside 0..1000
(`kMinBusDelayMs`/`kMaxBusDelayMs`, `audio_limits.h`); the same bound backs
the Settings field's `from`/`to`.

## Operation

`delayMs` travels through the existing `SetBusProcessing` operation (kind 20)
exactly like `equalizer` and `mode` beside it - there is no new operation,
capability bit, or method. A request replaces the bus's whole `BusProcessing`;
one out-of-range field refuses the whole rack and nothing moves, exactly as an
out-of-range equalizer value already did.

## Graph

`busProcessingActive` treats `delayMs > 0` the same as an enabled equalizer
or a non-Normal mode: it alone is enough to build the bus's filter-chain, so
a bus with a delay and nothing else still gets a chain. When active, the
graph worker appends PipeWire's builtin `delay` filter - a mono filter (one
"In" port, one "Out" port; verified against the installed PipeWire 1.6.8
filter-chain manual) - as two instances, `delay_l` and `delay_r`, one per
final output channel, linked after whichever equalizer chain the bus's
channel mode already selects for that channel. Each instance declares
`"max-delay" = 1.0`, matching the 1000 ms bound exactly, and
`"Delay (s)" = delayMs / 1000`. `delayMs == 0` adds no node at all: the graph
is byte-for-byte what it was before this schema.

As with the rack's equalizer and mode, this only ever applies to a physical
bus with a bound device; a virtual bus never gets a rack
(`aBusRackNeedsItsOwnSinkAndOnlyOnAPhysicalBus`, ADR-0180), so its `delayMs`
is carried and validated but never reaches the graph.

## Settings

The Mixer tab's bus rack (`AudioConsoleBusRack.qml`, opened from the bus
card's "Rack" toggle) shows a compact "Delay" field beside the equalizer: a
`Tk.NumberField` in whole milliseconds, 0..1000, with a Reset to 0 ms,
composed like the per-device latency field
(`AudioLatencyControl.qml`/[Audio1 schema 13](audio1-v13.md)). Both controls
dispatch a partial `{ delayMs: ... }` rack through the same
`setBusProcessing` request the equalizer knobs already use.
