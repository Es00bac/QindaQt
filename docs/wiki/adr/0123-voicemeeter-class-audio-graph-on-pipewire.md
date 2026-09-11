# ADR-0123: A Voicemeeter-class audio graph on PipeWire primitives

- **Status:** Accepted
- **Date:** 2026-09-10
- **Owners:** Audio service, Settings Audio route
- **Supersedes:** None
- **Superseded by:** None

## Context

VoiceMeeter Potato (VB-Audio) is the reference users ask for: a real mixing
console between applications and hardware — every app lands on its own strip
through virtual playback devices, strips carry gain/EQ/gate/compressor and
route through an 8×8 matrix to five physical outputs (A1–A5) and three
virtual outputs (B1–B3), with per-bus mixes, per-channel trim/EQ/delay
(0–500 ms) for speaker alignment, per-physical-output monitoring delay for
Bluetooth/HDMI lag, multi-device simultaneous playback, and virtual
recording devices for app-to-app audio and streamers. QindaQt's Audio1
service today exposes scalar volume, one default output, mute, and stream
moves over WirePlumber 0.5 — none of the graph.

Linux already owns the hard parts: PipeWire resamples anything to anything,
mixes unlimited devices simultaneously, and exposes channel volumes,
null-sink virtual devices, and `module-filter-chain` (delay, convolver,
upmix) natively. The clean QindaQt architecture is therefore a *control and
persistence layer* over the PipeWire graph, not a DSP engine — we never
write audio processing; we shape the graph and expose truthful controls.

## Decision

1. **Model**: keep Audio1's truth model (snapshot + fenced operations,
   epoch/revision lineage, fail-closed decoding) and extend the schema by
   versioned, additive slices. Devices gain per-channel truth
   (`channelVolumes`, `channelMap`); managed virtual devices
   (null sinks/sources with `media.name = QindaQt Virtual …` naming and
   `object.linger`) are first-class nodes with create/remove operations;
   capabilities are bits so old clients ignore what they do not know.
2. **Slices**, each independently shippable and testable against the real
   disposable PipeWire/WirePlumber runtime harness:
   - **S1 (this change)** — per-channel volumes and channel maps on every
     device and stream (surround control in software), and managed virtual
     devices: create/remove null sinks and sources that applications select
     like hardware (the B1–B3/VAIO concept). Multi-device playback already
     works by routing streams to any sink; S1 makes every sink first-class.
   - **S2** — output chains: per-output delay via `module-filter-chain`
     delay nodes (0–500 ms, live-updatable through node Props) for
     Bluetooth/HDMI alignment, plus optional per-channel trim before the
     hardware sink.
   - **S3** — virtual surround: `filter-chain` convolver sinks with HRIR
     profiles per output (speaker layouts with mixed wired/Bluetooth
     speakers), and `audioconvert` upmix/downmix mode selection.
   - **S4** — strips: per-application strips on virtual devices with
     send-to-many-outputs (loopback links with per-link gain = the routing
     matrix), EQ/gate/compressor through filter-chain, per-stream
     Level meters.
   - **S5** — network audio (VBAN-like) and preset snapshots.
3. **Backend discipline** (ADR-0014 unchanged): all WirePlumber/GLib work
   stays on the audio worker thread. Mutations use only public WirePlumber
   APIs — mixer-api `set-volume` with `channelVolumes` arrays for channel
   truth, `wp_node_new_from_factory` with the proven
   `adapter`/`support.null-audio-sink` recipe for virtual devices,
   `wp_global_proxy_request_destroy` for removal, `wp_core_sync` fences
   with Uncertain-on-epoch-change reporting.
4. **Persistence and naming**: managed virtual devices carry
   `node.name = qindaqt.virtual.<slug>` and persist across the service by
   re-adopting lingering nodes (object.linger) into the snapshot with a
   `virtual` provenance flag; nothing writes config files outside Settings1
   keys the Settings route already owns.
5. **Surface**: the Settings Audio route gains a Channels section (per
   channel fader with position labels) and a Virtual Devices section
   (create/remove, routed like hardware). The panel applet keeps its
   bounded rows and reads the same snapshot; new truth appears there as
   one aggregate volume until a later slice budgets strip UI.

## Consequences

- Surround control per device, virtual input/output devices for streamers
  and app-to-app audio, and honest multi-device routing land in S1 without
  inventing a mixer engine; delay and virtual surround follow as graph
  configuration, staying debuggable with stock `pw-dump`/`wpctl`.
- Audio1 v2 grows the wire snapshot; all in-repo consumers update in the
  same change, and the reference doc `audio1-v1.md` moves to a v2 page with
  the v1 contract documented as historical.
- Schema growth is capped by the existing bounds (128/128/256) plus
  per-device channel limits (≤32 channels, bounded names) so decoding stays
  fail-closed.
- Live runtime rows require the disposable PipeWire/WirePlumber harness;
  hosts without the binaries keep compiling (rows are conditional).
