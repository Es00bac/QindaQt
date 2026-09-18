# ADR-0208: console buses are OBS sources

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Platform (audio service, OBS bridge)
- **Supersedes:** None (builds on [ADR-0173](0173-the-mixing-console-slice-of-audio1.md) and the console endpoints of [ADR-0175](0175-virtual-strips-and-buses-are-nodes-the-console-owns.md))
- **Superseded by:** None

## Context

The reference console's value to a streamer is that OBS sees its buses as
inputs. QindaQt's console already exposes every virtual bus and strip as a
PipeWire node with a stable name (`qindaqt.console.<id>`, its `.source` and
`.monitor`), so OBS could capture them through its PulseAudio-protocol
capture sources; but a user would have to add one capture source per bus by
hand, pick the right node from a list of technical names, and redo it when a
bus is renamed or repointed at another device. OBS's own PipeWire plugin
captures video only; there is no upstream PipeWire audio capture.

## Decision

### An OBS module owns the console's sources

`obs-qindaqt` (`src/obs`, packaged as `media-plugins/obs-qindaqt`) registers
two audio source types, `qindaqt_console_bus` and `qindaqt_console_strip`.
Each wraps one private `pulse_input_capture` or `pulse_output_capture`
source pinned to the node named in its settings and republishes that audio
as its own, so the OBS mixer, recording and streaming see "QindaQt Bus B1 —
Chat" rather than a device id. The types are hidden from the "add source"
menu: the bridge creates and removes them.

### The Audio1 snapshot is the source of truth

On OBS's Qt main thread the module runs the shared audio client against the
session bus. Every snapshot is projected onto the set of sources the console
should be visible as (every bus, then every strip, in console order) and
diffed against the sources OBS holds, keyed by console id carried in the
source settings: removals first, then renames, retargets and creations. Each
source sits on a free mixer channel from 6 up (the frontend owns 1–5), so it
is live in the mixer and audible in recordings without a scene item. With a
frontend present the first sync waits for the scene collection to finish
loading, since loading replaces every source; a collection switch re-syncs
and adopts the restored sources instead of duplicating them.

### Capture follows the console's wiring

A virtual bus is captured from its `.source`, a physical bus from the
monitor of its current target device, a virtual strip from its sink's
monitor, a hardware strip from its capture device; a bus or strip without a
device yet exists silent and picks up the device when the console binds it.
The projection is pure (`src/obs/core`) and tested without libobs; the libobs
half is tested on a headless libobs with real sources and channels.

### The mapping is the control API

obs-websocket clients read the bridge through vendor `qindaqt`, request
`GetConsoleMapping` (buses and strips with console id, code, label, OBS
source name and kind, capture kind and node, mute and gain, plus the Audio1
state and snapshot epoch/revision) and event `ConsoleMappingChanged`. Mute
mirroring and scene-triggered macros stay with the obs-websocket client
(O10): the bridge never issues a console operation. The dock "QindaQt
Console" lists the sources with the console's own dBFS meters.

## Consequences

- OBS lists the console's buses and strips by name on first launch after the
  module installs, follows renames from Settings, and follows a bus retarget
  without the user touching OBS.
- Sources restored from a scene collection are adopted; a bridge source
  made by hand without a console id is left alone.
- No new daemon or bus name: the bridge is a client of Audio1 and a vendor of
  obs-websocket, both already running.
- The physical-bus monitor is what the speakers get after the bus rack; the
  virtual-strip monitor is the application's signal before the strip rack.

## Verification

- `qindaqt.obs-bridge-sync`: projection and sync plan (names, codes,
  capture rules, uniqueness, creations, renames, retargets, removals,
  duplicate and kind-mismatch repair).
- `qindaqt.obs-bridge-libobs`: headless libobs; source types create without
  a capture backend; a sync creates, renames in place, retargets and
  removes sources and frees their channels; restored sources are adopted.
- `qindaqt.obs-bridge-module-load`: the built module loads through
  `obs_open_module` and registers both types.
- Installed check (desktop, after the package cut): OBS lists the buses, a
  strip meters in OBS, a rename in Settings renames the OBS source.
