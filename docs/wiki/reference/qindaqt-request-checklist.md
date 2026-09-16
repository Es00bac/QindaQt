# Request checklist: audio, OBS, file manager, tray, roll-up, Wine

The single done-state for the 2026-09-16 request. A row is **done** only when
it is built, its tests pass, it is installed on the live session, and it is
committed. Anything less is stated as what it is.

## 1. Audio console (VoiceMeeter Potato parity)

The normative row-by-row list is
[VoiceMeeter Potato parity](voicemeeter-potato-parity.md); this section tracks
the work items that move rows there.

| Item | Status |
|---|---|
| Strips, buses, matrix with per-send gain, faders, mute/solo/mono ([ADR-0173](../adr/0173-the-mixing-console-slice-of-audio1.md)) | **done** (installed r2) |
| Real meters, console attached to hardware ([ADR-0174](../adr/0174-meters-are-a-stream-not-a-snapshot.md)) | **done** (installed r2) |
| Virtual strips and buses as real nodes ([ADR-0175](../adr/0175-virtual-strips-and-buses-are-nodes-the-console-owns.md)) | **done** (installed r2) |
| Persistence across restarts and logins ([ADR-0176](../adr/0176-the-console-remembers-itself.md)) | **done** (installed r2, document written on the live session) |
| Pan applied to the graph ([ADR-0177](../adr/0177-pan-is-balance-on-the-send.md)) | **done** (installed r2) |
| Per-strip / per-bus explicit device pins ([ADR-0178](../adr/0178-a-pin-is-a-name-not-a-handle.md)) | built and tested, commit pending |
| Gate, denoiser, compressor, limiter, EQ per strip | gap |
| Bus EQ and bus modes | gap |
| Recorder / player | gap |
| VBAN | gap |
| Macro buttons | gap |
| Presets | gap |
| Settings route complete for every control above; audio applet reflects the console | partial (console rack, faders, matrix, meters present) |
| Installed and running on the live session | r2 (through ADR-0177); pins pending r3 |

## 2. OBS integration

| Item | Status |
|---|---|
| Portal screen capture and virtual camera work out of the box ([ADR-0170](../adr/0170-survive-a-private-session-bus-for-dbus-units.md)) | done, installed |
| OBS with obs-websocket, PipeWire and v4l2 support installed | done (host packages) |
| Streaming stack packaged in the QindaQt overlay | gap |
| obs-websocket v5 client in the desktop | gap |
| OBS control applet | gap |
| Settings route for OBS | gap |
| QindaQt OBS plugins | gap |
| Audio-service bridge (console buses as OBS sources by default) | gap |

## 3. File manager

| Item | Status |
|---|---|
| Applications place, launch replaces the docked file manager ([ADR-0172](../adr/0172-applications-is-a-place-and-a-docked-window-can-replace-itself.md)) | committed |
| New-split gesture, mouse and keyboard, opening on the Applications tab | gap |

## 4. System tray

| Item | Status |
|---|---|
| StatusNotifier host registration so conformant items appear ([ADR-0166](../adr/0166-announce-a-status-notifier-host.md)) | committed |
| XEmbed to StatusNotifier proxy for Wine / Proton tray icons | gap |

## 5. Rolled-up container names

| Item | Status |
|---|---|
| A user-set name stays on the rolled-up badge ([ADR-0168](../adr/0168-a-generated-name-never-displaces-a-real-title.md)) | committed |
| Names persist across compositor restarts | gap |

## 6. Wine / Proton window identity

| Item | Status |
|---|---|
| Program behind an opaque window class reported with a real name and icon ([ADR-0169](../adr/0169-report-the-program-behind-an-opaque-window-class.md)) | committed |
| Steam `appmanifest` names for `steam_app_<id>` windows | gap |
| Icons extracted from the PE executable | gap |
