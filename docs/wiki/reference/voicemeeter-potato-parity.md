# VoiceMeeter Potato parity: the normative feature target

This page is the **acceptance checklist** for QindaQt's audio console. It is not
aspirational and it is not a summary of what QindaQt has: it is the reference
product's feature set, written down so that "done" has a definition and nothing
is quietly dropped.

The requirement, in the user's words: *"the original spec for the QindaQt audio
system used Voicemeeter Potato as the reference, in capability … make sure the
audio system is natively that powerful, not a cheap knockoff, but a real,
meaningfully similar implementation of the same features."*

Architecture is [ADR-0123](../adr/0123-voicemeeter-class-audio-graph-on-pipewire.md):
QindaQt is a control and persistence layer over the PipeWire graph. Where a
feature needs processing, it is realised with PipeWire `filter-chain` nodes, not
with DSP written here.

Status values: **done** (shipped and tested), **partial**, **planned** (designed,
not built), **gap** (no design yet).

## 1. Channel strips — inputs

Potato has **5 hardware input strips + 3 virtual input strips**. Every strip
carries the whole processing chain below.

| Feature | QindaQt design | Status |
| --- | --- | --- |
| 5 hardware input strips | `Strip` with `StripKind::HardwareInput`, following a capture device | **done** |
| 3 virtual input strips (VAIO / AUX / VAIO3) | `Strip` with `StripKind::VirtualInput` over managed null sinks, so each application gets its own fader | **done** |
| Per-strip device selection | Automatic by default ([ADR-0174](../adr/0174-meters-are-a-stream-not-a-snapshot.md)); a per-strip and per-bus pin by device name, persisted, with a picker in Settings ([ADR-0178](../adr/0178-a-pin-is-a-name-not-a-handle.md)) | **done** |
| Fader with dB readout | `Strip::gainDb`, one gain law ([ADR-0171](../adr/0171-one-gain-law-for-the-audio-console.md)) | **done** |
| Mute | `Strip::muted` | **done** |
| Solo | `Strip::soloed` + published `Console::soloActive` | **done** |
| Mono | `Strip::mono` | **done** |
| Pan / Intellipan | `Strip::pan` applied as balance on every send's two playback channels ([ADR-0177](../adr/0177-pan-is-balance-on-the-send.md)); the 2D Intellipan axis for surround buses is not built | partial |
| Per-channel gain/trim | `Strip::channelTrimDb` | planned |
| Level meters (per strip) | Real dBFS peak + RMS from a `pw_stream` capture per bound strip, streamed on the `Levels` signal ([ADR-0174](../adr/0174-meters-are-a-stream-not-a-snapshot.md)) with a falling peak marker | **done** |
| Bus assignment A1–A5, B1–B3 | `Strip::sends`, one `MatrixSend` per bus | **done** |
| Per-send gain | `MatrixSend::gainDb` — the matrix is not just on/off; realised as the loopback's volume | **done** |
| Gate | swh `gate` in the strip's filter-chain; threshold, attack, hold, release, range ([ADR-0179](../adr/0179-the-rack-is-one-value-per-strip.md)) | **done** |
| Denoiser | RNNoise (`noise_suppressor_mono`) first in the strip chain, voice-activity threshold dial ([ADR-0180](../adr/0180-a-bus-has-a-rack-too-and-a-strip-hears-clean.md)) | **done** |
| Compressor | swh `sc4m`; threshold, ratio, attack, release, knee, makeup ([ADR-0179](../adr/0179-the-rack-is-one-value-per-strip.md)) | **done** |
| Limiter | swh `hardLimiter` brick wall at the ceiling; no release ([ADR-0179](../adr/0179-the-rack-is-one-value-per-strip.md)) | **done** |
| Strip EQ (3-band) | PipeWire builtin biquads: low shelf, peaking mid with Q, high shelf ([ADR-0179](../adr/0179-the-rack-is-one-value-per-strip.md)) | **done** |
| Karaoke modes (K-m, K-1, K-2, K-v) | mid/side cancellation modes on a stereo strip | gap |
| Audibility | voice-band emphasis | gap |
| Reverb send | shared FX bus + per-strip send | gap |
| Delay send | shared FX bus + per-strip send | gap |
| Pitch shift (virtual strips) | `filter-chain` pitch node | gap |
| Patch insert (per channel) | insert point in the strip's chain | gap |

## 2. Buses — outputs

Potato has **5 physical buses (A1–A5) + 3 virtual buses (B1–B3)**.

| Feature | QindaQt design | Status |
| --- | --- | --- |
| 5 physical buses | `Bus` with `BusKind::Physical` driving a real sink | **done** |
| 3 virtual buses | `Bus` with `BusKind::Virtual` as managed null sinks other apps record from | **done** |
| Per-bus device selection | `SetBusTarget` | planned |
| Fader with dB readout | `Bus::gainDb` | **done** |
| Mute / Mono | `Bus::muted`, `Bus::mono` | **done** |
| Level meters | Real dBFS peak + RMS, read from the bus device's monitor ([ADR-0174](../adr/0174-meters-are-a-stream-not-a-snapshot.md)) | **done** |
| Bus EQ | Three-band equalizer per physical bus, one per channel, in the bus chain ([ADR-0180](../adr/0180-a-bus-has-a-rack-too-and-a-strip-hears-clean.md)) | **done** |
| Bus modes | Stereo subset: normal, swap, left-to-both, right-to-both, as the bus chain's output mapping ([ADR-0180](../adr/0180-a-bus-has-a-rack-too-and-a-strip-hears-clean.md)); surround upmixes not modelled | partial |
| Per-bus monitoring delay (Bluetooth/HDMI alignment) | `filter-chain` delay, 0–500 ms — ADR-0123 slice S2 | planned |
| Virtual surround / HRIR | convolver sinks — ADR-0123 slice S3 | planned |

## 3. Recorder and player

| Feature | QindaQt design | Status |
| --- | --- | --- |
| Multitrack recorder (up to 8 channels) | capture node fanned from selected strips/buses | gap |
| Record format selection (WAV/AIFF/MP3…) | encoder selection on the recorder | gap |
| Source selection (any bus or strip set) | recorder input matrix | gap |
| Transport: record / play / pause / loop | recorder transport state on the wire | gap |
| File playback into buses (tape/cassette) | playback node routed like a strip | gap |

## 4. Network audio — VBAN

| Feature | QindaQt design | Status |
| --- | --- | --- |
| 8 incoming VBAN streams | ADR-0123 slice S5 | gap |
| 8 outgoing VBAN streams | ADR-0123 slice S5 | gap |
| Per-stream IP, port, name, sample rate, format, quality | VBAN stream config on the wire | gap |
| VBAN text / MIDI / service channels | later than audio streams | gap |

## 5. Control surfaces and automation

| Feature | QindaQt design | Status |
| --- | --- | --- |
| Macro buttons with scripting | QindaQt applet + a bounded action vocabulary | gap |
| MIDI in/out mapping to console controls | MIDI binding layer over the operation kinds | gap |
| Remote control API | `org.qindaqt.Audio1` **is** the remote API, and it is a better one than a DLL | partial |
| System tray presence | the QindaQt audio applet | partial |
| Presets / saved configurations | the console persists its own document; named presets are not built | partial |
| Startup configuration | applied at service start | planned |

## 6. Engine and system

| Feature | QindaQt design | Status |
| --- | --- | --- |
| Per-driver buffering (MME/WDM/KS/ASIO) | not applicable: PipeWire owns quantum and period | n/a by design |
| Internal sample rate selection | PipeWire graph rate | planned |
| Engine restart | service restart without losing console state | planned |
| ASIO virtual driver | not applicable on Linux; the virtual sinks are the equivalent | n/a by design |
| Patch composite (PC1–PC8) | arbitrary channel routing to composite outputs | gap |

## 7. What QindaQt has today

Audio1 v3 ([ADR-0173](../adr/0173-the-mixing-console-slice-of-audio1.md))
publishes a real console: 5 hardware and 3 virtual input strips, A1–A5 and
B1–B3 buses, and a rectangular routing matrix whose every cell carries its own
gain. Each enabled send is a live `libpipewire-module-loopback` whose playback
node's volume **is** that cell's fader, so the matrix routes real audio rather
than describing an intention. Mute, solo and bus mute silence an edge without
tearing it down. The console persists, survives its devices disappearing, and
is operated from the Settings Audio route.

Meters are real. Hardware strips and physical buses attach themselves to the
graph out of the box, and each bound endpoint is read by its own capture stream
whose peak and RMS stream to every surface on a dedicated `Levels` signal
([ADR-0174](../adr/0174-meters-are-a-stream-not-a-snapshot.md)).

Virtual strips and buses are real nodes the console owns
([ADR-0175](../adr/0175-virtual-strips-and-buses-are-nodes-the-console-owns.md)):
an application that picks "Virtual Input" as its output gets its own fader, and
a streamer's software records a submix from "B1".

Each strip has a rack — gate, compressor, three-band EQ, limiter — realised as
one filter-chain per strip and persisted with the console
([ADR-0179](../adr/0179-the-rack-is-one-value-per-strip.md)).

A strip is denoised before its gate, and a physical bus has its own rack
([ADR-0180](../adr/0180-a-bus-has-a-rack-too-and-a-strip-hears-clean.md)).

What is emphatically not here yet: the recorder, VBAN and macro buttons, and
the surround bus modes.

This page exists so that distance is visible rather than implied.
