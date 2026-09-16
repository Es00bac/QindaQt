# ADR-0179: the rack is one value per strip

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (fills the largest gap named in [ADR-0173](0173-the-mixing-console-slice-of-audio1.md))
- **Superseded by:** None

## Context

Between a strip's input and its fader the reference console has a rack: a
gate, a compressor, an equalizer and a limiter, each switchable, each with its
own dials. [VoiceMeeter Potato parity](../reference/voicemeeter-potato-parity.md)
listed every one of them as a **gap**, and they are where a console's sound is
made — a streamer's microphone is gated, compressed and limited before anyone
hears it.

PipeWire's `filter-chain` module can host such a rack, but the host had no
processor plugins at all: LADSPA and LV2 directories were empty, PipeWire was
built without LV2 support, and its builtin filters cover biquads, convolution
and arithmetic — enough for an equalizer, nothing for dynamics.

## Decision

### The wire

`StripProcessing` (schema 6): `GateSettings`, `CompressorSettings`,
`EqualizerSettings`, `LimiterSettings`, in the order the graph applies them.
Every block carries `enabled` and keeps its dials while off, so switching a
compressor off and on restores the user's settings rather than defaults.
Times are milliseconds, levels dBFS, gains dB, with bounds in `audio_limits.h`
shared by the console gate, the model and the client's preflight so all three
refuse exactly the same values.

### One operation

`SetStripProcessing` (kind 19) replaces the **whole** rack. A client always
holds the current rack and sends it back with one control changed, which keeps
the surface to one method instead of twenty and makes the admission rule
simple: any value out of range refuses the whole rack and nothing moves. A
half-applied rack — the compressor at the new setting, the gate at the old —
would be worse than a rejected one.

The rack is part of the strip's document ([ADR-0176](0176-the-console-remembers-itself.md));
a document whose rack fails the bounds keeps the strip's current rack rather
than half of the new one.

### Realisation

One `filter-chain` module per strip whose rack has any block enabled, inserted
between the strip's device and its sends: the chain captures the device (or,
for a virtual strip, its sink's monitor) and presents the result as a virtual
**source**, `qindaqt.console.<strip id>.processed`, which the strip's sends and
meter then read instead of the device. It is a source, not a sink: a
filter-chain's playback side is an *output* stream, and PipeWire refuses
`Audio/Sink` on one. A strip with every block off has no chain and its sends
read the device directly, so the default console costs nothing.

The chain is instantiated once per channel, so every plugin in it must be
mono. Dynamics come from `swh-plugins` (LADSPA): `gate`, `sc4m` (the mono SC4)
for the compressor, `hardLimiter` for the limiter — a runtime dependency the
package now declares. The stereo `sc4` and lookahead limiter have no `Input`
port and PipeWire refuses the whole graph on the first link. The equalizer is
three of PipeWire's builtin biquads (low shelf, peaking, high shelf), which
need no plugin. Each plugin's own control range is narrower than the wire's;
values are clamped to the plugin, so the console offers one scale and the
graph only sees what the plugin was written for. The hard limiter has no
release; the wire keeps `releaseMs` for a lookahead limiter with one.

A changed rack is a reload of the chain, and the sends follow it one graph
event later: a send is pointed at the processed source only once that node
exists, because a send aimed at a name the graph does not have yet is linked
by PipeWire to the *default* device and never moved back. Every send and
chain now carries `node.dont-fallback`, so an absent target leaves a side
unlinked rather than reading whatever is default.

Two lifetime rules fell out of making this work. A send is diffed by its
*arguments*, not its name: a send whose device moved — a re-pin, a rack
starting — is a different send under the same name, and diffing by name
alone kept it on the old device. And every loaded module is watched for
PipeWire destroying it itself (a stream that could not connect); the record
is dropped only if it still holds that module, since the key may already
belong to its replacement. Destroying a module inside the dispatch that
delivered a graph event is deferred to an idle on the worker loop.

## Consequences

- A strip can be gated, compressed, equalized and limited; the settings
  survive restarts with the rest of the console.
- `swh-plugins` becomes part of a QindaQt install. It is LADSPA, which
  PipeWire loads unconditionally; LV2 would have required rebuilding PipeWire.
- The denoiser remains separate: it needs the rnnoise LADSPA plugin, which is
  not in the main Portage tree and is packaged in the QindaQt overlay as its
  own step.

## Revisit when

- The denoiser lands (rnnoise as a fifth block, first in the chain).
- Bus-side processing (bus EQ, bus modes) reuses the chain shape on the bus's
  sink rather than the strip's source.
- A sidechain input for the gate; the reference console offers one.
