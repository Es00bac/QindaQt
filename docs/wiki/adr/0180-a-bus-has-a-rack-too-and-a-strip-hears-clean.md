# ADR-0180: a bus has a rack too, and a strip hears clean

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (extends [ADR-0179](0179-the-rack-is-one-value-per-strip.md))
- **Superseded by:** None

## Context

[ADR-0179](0179-the-rack-is-one-value-per-strip.md) gave every strip a gate,
compressor, equalizer and limiter, and left two rows of the parity page open:
the denoiser — the block a streamer reaches for first — and the reference
console's bus-side EQ and bus modes.

## Decision

### The denoiser is the first block

`DenoiserSettings` joins `StripProcessing` at the front (schema 7): RNNoise runs
before the gate so the gate judges cleaned audio rather than room noise. It is
`noise-suppression-for-voice`'s LADSPA `noise_suppressor_mono`, one per channel
like the rest of the chain, with the voice-activity threshold as its one dial
(0–99 %, the plugin's own integer range). The package is a runtime dependency
alongside `swh-plugins`.

### A physical bus gets a rack

`BusProcessing` (a three-band equalizer and a channel mode) rides on every
`Bus`; `SetBusProcessing` (kind 20) replaces it whole, admitted and persisted
exactly as a strip's rack is. Only a **physical** bus realises it: a virtual
bus's sink *is* what other applications record, and the reference console has
no rack on its B buses either.

A physical bus has no node of its own — its sends play straight into the
device. A rack needs somewhere to sit, so a bus with an active rack gets a
**pre-rack sink**, `qindaqt.console.<bus id>`, created like a strip's sink and
declared as an endpoint only while the rack is active, so the default console
adds no devices. The sends then play into that sink and the rack's chain
captures its monitor and plays into the device. Switching the rack off
withdraws the sink: it is destroyed through the registry — a lingering object
outlives the proxy that made it — and the cleanup is driven by the graph, not
by this run's proxies, so a leftover from a previous run of the service is
removed too.

### The bus chain is stereo on purpose

A strip's chain is one mono graph instantiated per channel. A bus's is an
explicit stereo graph, one equalizer per channel, because the modes are about
*which channel goes where* — something a duplicated graph cannot express. The
mode is the chain's output mapping: `Normal`, `SwapChannels`, `LeftToBoth`,
`RightToBoth`, the stereo-desktop subset of the reference console's bus modes;
its surround upmixes are not modelled. An equalizer that is off still exists
with flat gains, since the mode alone can need the chain.

## Consequences

- A microphone strip can be denoised, gated, compressed, equalized and limited
  before anyone hears it, and the whole rack survives a reboot.
- A bus can be equalized and its channels swapped or summed to one side.
- Two more runtime dependencies for a QindaQt install; both LADSPA, which
  PipeWire loads unconditionally.
- Verified on the live graph: the denoiser took a microphone's room noise from
  about −30 dB to −53 dB at the strip meter; a bus rack on A2 captured the
  bus's sink and played into the HDMI device with the send moved onto the
  sink, and switching it off removed the sink.

## Revisit when

- The surround bus modes are wanted; the chain shape supports more outputs.
- A bus limiter or compressor is wanted; the bus chain takes further blocks
  exactly as the strip chain does.
