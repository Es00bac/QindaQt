# ADR-0288: Per-device latency offsets through Audio1, and a compact Audio page

- **Status:** Accepted
- **Date:** 2026-09-28
- **Owners:** Audio service and Settings Audio route
- **Supersedes:** None
- **Superseded by:** None

## Context

Bluetooth, HDMI, USB and network (VBAN) paths each add their own delay, so a
video plays out of sync on one device and fine on another. The owner asked for
an easy per-device adjustment and, in the same request, a denser Settings →
Audio page built from QindaTK's compact controls.

PipeWire already has a native mechanism. Verified on qinda against the
installed PipeWire 1.6.8 and WirePlumber 0.5.15:

- ALSA sink and source nodes publish `latencyOffsetNsec` (a signed 64-bit
  nanosecond value) in their node `Props` and declare a `0..2 s` range in
  `PropInfo`; Bluetooth sinks publish it with the whole signed range.
- WirePlumber itself writes node `Props { latencyOffsetNsec }` for Bluetooth
  MIDI, and its `state-routes` script persists the same value per device route.
- The offset changes the latency a node *reports*. Players (A/V sync) and
  PipeWire's `combine-stream` latency compensation use it; PipeWire does not
  insert delay into ordinary links because of it.
- A Bluetooth device re-emits its own route offset onto the node whenever route
  properties are applied (on connect), overwriting a node-level value once.
- `support.null-audio-sink` nodes (virtual devices, console buses) and
  Bluetooth *sources* publish no offset.

The route-level alternative (`Route` param `props`) would need the device, the
active route and its card-profile device for every node, and would make
WirePlumber's state file a second owner of the value; it does not exist for
nodes without routes.

## Decision

- **Mechanism.** Audio1 writes node `Props { latencyOffsetNsec }` and reads
  the node's `Props` and `PropInfo` back. A device whose node publishes no
  offset reports it as *absent*; Audio1 never synthesizes one.
- **Wire contract (schema 13, additive).** `Device` appends
  `latencyOffsetKnown`, `latencyOffsetNs`, `canSetLatencyOffset`,
  `latencyOffsetMinNs`, `latencyOffsetMaxNs` (D-Bus `bxbxx`); capability bit 11
  `SetLatencyOffset`; operation kind 30 and method
  `SetLatencyOffset((tt) device, x offsetNs)`. The settable range is the node's
  declared range clipped to ±2 s. One admission rule
  (`latencyOffsetAdmitted`) is shared by the service, the client preflight and
  the Settings route. See [Audio1 schema 13](../reference/audio1-v13.md).
- **Persistence and policy.** Audio1 owns the value. `LatencyStore` keeps
  `node.name → offset` in `$XDG_CONFIG_HOME/qindaqt/audio-latency.json`
  (atomic replace, bounded, invalid entries skipped). `SetLatencyOffset`
  completes in the coordinator like a console change: it remembers the value
  (0 included, so a reset survives WirePlumber restoring an older value) and
  declares the whole map to the backend (`applyLatencyOffsets`).
- **Application.** The graph worker writes a declared offset to every present
  device node with that `node.name` when the node's declared range admits it.
  Per node serial it keeps a write budget of three: it does not resend while
  its last write is unechoed, answers a reset (the Bluetooth re-emission) with
  one more write, and after three leaves a node that keeps overriding the
  value alone, with the node's own value visible. A new declaration resets the
  budget; a departed serial forgets its memory. With nothing declared for a
  device, Audio1 never writes it.
- **Settings.** Each device row shows a compact `Tk.NumberField` in whole
  milliseconds with a Reset to 0 ms, and hides both when the offset is absent.
  A read-only offset is shown unclamped. The field is bounded numeric entry,
  not free text. A dispatched target stays on screen until a snapshot reports
  it, a refusal returns the row to device truth, and a device that never
  reports the value returns to its own value after 3 s with a notice.
- **Compact page.** The Audio page's Devices tab uses QindaTK small controls
  (`Tk.Button`, `Tk.Switch`, `Tk.Slider`, `Tk.NumberField`, `Tk.SectionHeader`)
  and tighter spacing: one line names a device, one carries its level and
  mute, one its offset and channels. The healthy-service card collapses to a
  "Ready" word beside the heading. Every control, object name, keyboard path
  and accessible name is kept; section descriptions remain as accessible
  descriptions only.

## Consequences

- Clients and services must both be schema 13; an older peer fails closed on
  the snapshot version, as with every earlier schema step.
- The offset aligns what players and latency-compensating PipeWire modules
  see. It does not delay audio routed through the console's plain loopbacks,
  so two speakers playing the same console bus are not realigned by it; that
  would need a delay stage in the graph (see Revisit when).
- The worker now caches node `Props` (`WP_PIPEWIRE_OBJECT_FEATURE_PARAM_PROPS`)
  and enumerates `PropInfo` once per device node lifetime; device-node
  `params-changed` triggers a rebuild only when the offset changed.
- Focused evidence: `qindaqt.audio-latency-offsets` (store, projection,
  reconcile policy, coordinator), `qindaqt.audio-wireplumber-latency-runtime`
  (an ALSA `null` PCM sink in a private PipeWire: read, apply, answer a reset,
  never fight when undeclared), `qindaqt.audio-protocol`,
  `qindaqt.audio-qt-transport`, `qindaqt.settings-audio-latency`,
  `qindaqt.settings-audio-latency-page`, and the existing Settings Audio rows
  with geometry expectations updated where the compact layout moved content.
- Unqualified: real Bluetooth route re-emission timing, HDMI/USB hardware, and
  audible or visible A/V alignment in a live session.

## Revisit when

- A user needs two simultaneous outputs aligned in sound, not only reported
  latency: add an explicit per-bus delay stage to the console graph.
- PipeWire starts insisting on route-level offsets for Bluetooth or ALSA (a
  node-level write no longer sticks after the one re-emission), or a
  WirePlumber release persists node-level offsets itself.

## Addendum (2026-09-28): a real per-bus delay stage

The first "Revisit when" condition above was met the same day: the owner
asked for two speakers fed from different buses, or a bus and a VBAN peer
fed from that bus, to play in sync, not just report matching latency. The
per-device offset above cannot do this - it never inserts delay into console
routing - so this addendum adds one.

- **Model.** Each bus's rack (`BusProcessing`, ADR-0180) gains `delayMs`, a
  whole-millisecond integer bounded `kMinBusDelayMs`..`kMaxBusDelayMs`
  (0..1000, `audio_limits.h`), default 0. It is carried, validated, and
  persisted exactly like the rack's equalizer: through the existing
  `SetBusProcessing` operation (kind 20) and the console's existing mutation
  capability - no new operation, capability bit, or method. `busProcessingActive`
  treats `delayMs > 0` the same as an enabled equalizer or a non-Normal mode:
  it alone is enough to build the bus's filter-chain.
- **Wire contract (schema 14, additive).** `BusProcessing` appends `delayMs`
  (D-Bus `i`) after `mode`, its existing last field. See
  [Audio1 schema 14](../reference/audio1-v14.md).
- **Graph.** The graph worker appends PipeWire's builtin `delay` filter
  (verified against the installed PipeWire 1.6.8 filter-chain manual,
  `libpipewire-module-filter-chain(7)`: a mono filter, one "In" port and one
  "Out" port) when `delayMs > 0`: one instance per final output channel
  (`delay_l`, `delay_r`), linked after whichever equalizer chain the bus
  mode already selects for that channel, `"max-delay" = 1.0` matching the
  1000 ms bound exactly, `"Delay (s)" = delayMs / 1000`. Zero adds no node at
  all: the graph is byte-for-byte what it was before this addendum.
- **Scope: physical buses only, like the rack it lives in.** A bus rack -
  equalizer, mode, and now delay - is built only for a physical bus with a
  bound device; a virtual bus's own sink would have to become a post-rack
  node to gain one (`aBusRackNeedsItsOwnSinkAndOnlyOnAPhysicalBus`), which is
  unrelated, larger surgery this addendum does not attempt. A bus driving an
  outgoing VBAN stream shares that bus's own delay, so aligning a bus's local
  device against its VBAN peer's downstream playback is possible; aligning
  two peers independently, or delaying a strip rather than a bus, is not.
- **Settings.** The Mixer tab's bus rack (`AudioConsoleBusRack.qml`, opened
  from the bus card's existing "Rack" toggle, beside the equalizer) gains a
  compact "Delay" field: a `Tk.NumberField` in whole milliseconds with a
  Reset to 0 ms, composed like `AudioLatencyControl.qml`'s device field, and
  dispatched through the same whole-rack `setBusProcessing` request the
  equalizer already uses.

Focused evidence: `qindaqt.audio-processing`
(`aBusDelayAppendsMonoNodesAfterTheMode`: the graph string for 0, 25 and
1000 ms, and delay alone building the chain), `qindaqt.audio-console-model`
(`busDelayIsBoundedAppliedAndPersists`: bounds and a JSON round trip), and
`qindaqt.settings-audio-console-page` (`consoleBusRackDelayDispatches`: the
Settings control, offscreen). Unqualified: audible alignment on real hardware
or a live VBAN peer, and Bluetooth/HDMI/USB devices, as before.
