# OBS console bridge

## 2026-09-19 authority diagnosis

At base `01e919f6`, the bridge retains `m_lastSnapshot` and active capture
sources when AudioClient loses its owner and clears its snapshot.
`onState` only updates a status label. A subsequent scene-collection change
can therefore reapply the retired owner's wiring, and previously bound
hardware captures keep running without authoritative console state. The
correction must forget retained wiring and remove the bridge-managed sources
when AudioClient has no snapshot, while retaining a labelled stale snapshot
when the client itself still holds one. This is source evidence, recorded
before changes; no OBS process or live audio graph was changed.

The candidate follows AudioClient's snapshot ownership on state changes,
clears the mapping and dock when authority is lost, and removes bridge-owned
sources when the frontend is ready. A collection that finishes loading without
current Audio1 authority also has its restored bridge sources removed.

`obs-qindaqt` is an OBS Studio module that shows the QindaQt audio console
to OBS ([ADR-0208](../adr/0208-console-buses-are-obs-sources.md)). It is
built from `src/obs` against the installed `libobs`, `obs-frontend-api` and
the `obs-websocket` API header, and packaged as `media-plugins/obs-qindaqt`.
It is a client of Audio1 and a vendor of obs-websocket; it runs no daemon and
issues no console operation.

## What OBS sees

| Console object | OBS source | Type | Captures |
| --- | --- | --- | --- |
| Virtual bus (B1–B3) | `QindaQt Bus B1 — <label>` | `qindaqt_console_bus` | `qindaqt.console.bus.b1.source` (input capture) |
| Physical bus (A1–A5) | `QindaQt Bus A1 — <label>` | `qindaqt_console_bus` | the target device's `<node.name>.monitor` (output capture) |
| Virtual strip | `QindaQt Strip Virtual 1 — <label>` | `qindaqt_console_strip` | `qindaqt.console.strip.virtual.1.monitor` (output capture) |
| Hardware strip | `QindaQt Strip Hardware 1 — <label>` | `qindaqt_console_strip` | the capture device's `node.name` (input capture) |

The label part is omitted when the console label is empty or only restates
the code; two sources that would share a name carry the console id in
brackets. A bus or strip whose device is not bound yet exists silent with an
empty capture node and picks the device up on the next snapshot.

Each source wraps one private PulseAudio-protocol capture source (PipeWire
serves that protocol) and republishes its audio, and sits on a free mixer
channel from 6 up, so it is live in the mixer and in recordings without a
scene item. The types are hidden from OBS's "add source" menu; the bridge
creates, renames, retargets and removes them from every Audio1 snapshot.
Sources restored from a scene collection are adopted by console id, not
duplicated; a bridge-typed source without a console id was made by hand and
is left alone, and a source OBS has already removed is never adopted even
while some holder still references it.

Source settings (also the vendor request's fields): `qindaqt_console_id`,
`qindaqt_code`, `qindaqt_label`, `qindaqt_capture_kind` (`input`, `monitor`,
`none`), `qindaqt_capture_device`.

### Existing mixing-policy limitation

Every mapped source is attached to a mixer channel by the existing ADR-0208
policy. A signal routed from a strip to a bus can therefore enter OBS through
both captures at once, and a raw hardware/virtual-strip capture precedes that
strip's rack and send gains. Console mute/gain values in the vendor mapping
describe Audio1; this bridge does not apply them as an additional OBS mixer
mute/gain. Users must select the intended captures in OBS's mixer to avoid
duplicate or raw-strip audio. The authority/lifetime correction does not
change source activation defaults or claim a consolidated recording mix.

## Lifecycle inside OBS

`obs_module_load` registers the source types. `obs_module_post_load` (after
every module, on the Qt main thread) starts the bridge: the audio client on
the session bus, the "QindaQt Console" dock, and the obs-websocket vendor.
With a frontend the first sync waits for `FINISHED_LOADING` and every
`SCENE_COLLECTION_CHANGED` re-syncs; `SCENE_COLLECTION_CHANGING`, `EXIT` and
`SCRIPTING_SHUTDOWN` pause it. Without a frontend (a headless libobs host)
the types still work and the bridge stays off.

## The control API

Vendor `qindaqt`, request `GetConsoleMapping`:

```json
{
  "bridgeVersion": 1,
  "audioState": "ready",
  "reasonCode": "",
  "epoch": 7,
  "revision": 42,
  "buses": [{"consoleId": "bus.b1", "code": "B1", "label": "Chat",
             "sourceName": "QindaQt Bus B1 — Chat", "sourceKind": "qindaqt_console_bus",
             "captureKind": "input", "captureDevice": "qindaqt.console.bus.b1.source",
             "muted": false, "gainDb": 0.0}],
  "strips": []
}
```

Event `ConsoleMappingChanged` carries the same payload whenever the mapping
or the Audio1 state changes. The O10 Settings bus-mapping table and the
obs-websocket client read this; mute mirroring and scene macros are theirs.

## Dock

"QindaQt Console" lists every bridge source with the console's own dBFS peak
meter (from Audio1's level readings, not OBS's mixer) and the Audio1 state.

## Building and packaging

`src/obs/CMakeLists.txt` configures alone (`cmake -S src/obs`), which is how
the ebuild builds it (`CMAKE_USE_DIR=src/obs`); a repository build adds it and
skips it quietly when the OBS development files are absent. The module
installs to `<libdir>/obs-plugins/obs-qindaqt.so`. The ebuild's
`QINDAQT_COMMIT` and Manifest are set at the package cut like the desktop
package's.

## Verification

`qindaqt.obs-bridge-sync` (pure projection and plan), `qindaqt.obs-bridge-libobs`
(headless libobs: create, rename in place, retarget, remove, channel
attachment, adoption of restored sources) and `qindaqt.obs-bridge-module-load`
(the built module loads through `obs_open_module`). The installed check on a
desktop with the package: OBS lists the buses, audio from a strip meters in
OBS, a rename in Settings renames the OBS source.
