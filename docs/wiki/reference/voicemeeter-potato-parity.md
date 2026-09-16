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
| 5 hardware input strips | `Strip` with `StripKind::HardwareInput`, following a capture device | planned |
| 3 virtual input strips (VAIO / AUX / VAIO3) | `Strip` with `StripKind::VirtualInput` over managed null sinks, so each application gets its own fader | planned |
| Per-strip device selection | `SetStripSource` against the device slice | planned |
| Fader with dB readout | `Strip::gainDb`, one gain law ([ADR-0171](../adr/0171-one-gain-law-for-the-audio-console.md)) | planned |
| Mute | `Strip::muted` | planned |
| Solo | `Strip::soloed` + published `Console::soloActive` | planned |
| Mono | `Strip::mono` | planned |
| Pan / Intellipan | `Strip::pan`; the 2D pan needs a second axis for surround | partial design |
| Per-channel gain/trim | `Strip::channelTrimDb` | planned |
| Level meters (per strip) | `Strip::level`, dBFS peak + RMS | planned |
| Bus assignment A1–A5, B1–B3 | `Strip::sends`, one `MatrixSend` per bus | planned |
| Per-send gain | `MatrixSend::gainDb` — the matrix is not just on/off | planned |
| Gate | `filter-chain` gate node; threshold, attack, hold, release, sidechain BP | gap |
| Denoiser | `filter-chain` (rnnoise is already an OBS/PipeWire-adjacent dependency) | gap |
| Compressor | `filter-chain` compressor; ratio, attack, release, knee, GI/GO | gap |
| Limiter | `filter-chain` limiter with dB ceiling | gap |
| Strip EQ | per-strip parametric EQ cells | gap |
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
| 5 physical buses | `Bus` with `BusKind::Physical` driving a real sink | planned |
| 3 virtual buses | `Bus` with `BusKind::Virtual` as managed null sinks other apps record from | planned |
| Per-bus device selection | `SetBusTarget` | planned |
| Fader with dB readout | `Bus::gainDb` | planned |
| Mute / Mono | `Bus::muted`, `Bus::mono` | planned |
| Level meters | `Bus::level` | planned |
| Bus EQ (6-band parametric, per channel) | `filter-chain` EQ per bus, with memory slots | gap |
| Bus modes (Normal, Amix, Bmix, Repeat, Composite, TV Mix, Upmix 2.1/4.1/6.1, Center/LFE/Rear only) | channel-matrix mode per bus | gap |
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
| Presets / saved configurations | console state persisted through Settings1 | planned |
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

Audio1 v2 (ADR-0123 slice S1) publishes devices, streams, per-channel volumes
and channel maps, and manages virtual devices. The one gain law
([ADR-0171](../adr/0171-one-gain-law-for-the-audio-console.md)) is shipped. The
console model in section 1 and 2 is being built; everything marked **gap** above
has no implementation.

Measured against this page, the honest completion figure is small. The page
exists so that figure is visible rather than implied.
