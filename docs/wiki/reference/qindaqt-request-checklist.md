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
| Per-strip / per-bus explicit device pins ([ADR-0178](../adr/0178-a-pin-is-a-name-not-a-handle.md)) | committed (14 files on main cite it); not confirmed on the live session |
| Gate, compressor, limiter, EQ per strip ([ADR-0179](../adr/0179-the-rack-is-one-value-per-strip.md)) | **done** (installed r4) |
| Denoiser per strip ([ADR-0180](../adr/0180-a-bus-has-a-rack-too-and-a-strip-hears-clean.md)) | **done** (installed r6; a denoiser-only rack loads on the packaged service) |
| Bus EQ and bus modes ([ADR-0180](../adr/0180-a-bus-has-a-rack-too-and-a-strip-hears-clean.md)) | **done** (installed r5; stereo modes, surround upmixes not modelled) |
| Recorder ([ADR-0184](../adr/0184-the-recorder-is-a-stream-and-a-writer-thread.md)) | committed (bus to FLAC/WAV; 12 files on main); the installed `qindaqt-audio-service` carries it. Player still not built |
| VBAN ([ADR-0185](../adr/0185-vban-is-a-document-and-two-threads.md)) | committed (audio sub-protocol, document-defined streams; 13 files on main); the installed `qindaqt-audio-service` carries it |
| Macro buttons ([ADR-0183](../adr/0183-a-macro-button-is-a-list-of-console-operations.md)) | committed (file-defined; run from Settings; 9 files on main); the installed `qindaqt-audio-service` carries it |
| Presets ([ADR-0182](../adr/0182-a-preset-is-the-console-under-a-name.md)) | **done** (installed r6; verified round trip on the packaged service) |
| Settings route complete for every control above | **done** for every row above: strips, buses, matrix, meters, pins, strip and bus racks |
| Audio applet reflects the console ([ADR-0181](../adr/0181-the-tray-rides-the-console.md)) | **done** (installed r6) |
| Installed and running on the live session | r6 (through ADR-0182 and the active-rack fix) |

**On the four rows above (2026-09-21).** They read "build/commit pending the
suite" long after the code had landed. "Committed" is verified from the tree:
each ADR is cited by 9-14 files on main. "Installed" is verified only where
named: a distinctive symbol appears in the installed
`/usr/bin/qindaqt-audio-service`. The device-pin row is deliberately left
unconfirmed on the session rather than guessed - a first attempt to check it
matched the substring "pin" inside "Typing" and would have claimed whatever
the reader wanted.

## 2. OBS integration

| Item | Status |
|---|---|
| Portal screen capture and virtual camera work out of the box ([ADR-0170](../adr/0170-survive-a-private-session-bus-for-dbus-units.md)) | done, installed |
| OBS with obs-websocket, PipeWire and v4l2 support installed | done (host packages) |
| Streaming stack packaged in the QindaQt overlay | **done** — `media-plugins/obs-qindaqt-0.1.0_pre20260921-r2` is installed on this host, owning exactly one file (`/usr/lib64/obs-plugins/obs-qindaqt.so`) |
| obs-websocket v5 client in the desktop ([ADR-0201](../adr/0201-one-obs-websocket-client-for-the-desktop.md), [ADR-0202](../adr/0202-qindaqt-provisions-obs-and-owns-one-secret.md)) | **done** (installed: `libqindaqt_obs_client.a`; 5 ctest rows) |
| OBS control applet | **done** (installed: `/usr/share/qindaqt/applets/obs.json`; placed in the `qindaqt` stock profile) |
| Settings route for OBS | **done** — it is the **streaming** route (`src/apps/settings/streaming`), wired through `SettingsRouteHost.qml` and `Main.qml`; the row read `gap` only because it was looked for under the name "obs" |
| QindaQt OBS plugins ([ADR-0208](../adr/0208-console-buses-are-obs-sources.md)) | **done** — now installed by `media-plugins/obs-qindaqt`, which is its only owner |
| Audio-service bridge (console buses as OBS sources by default) ([ADR-0208](../adr/0208-console-buses-are-obs-sources.md)) | built and committed; ships inside the plugin above, so it reaches a machine when that package does |

**Ownership note (2026-09-21).** Five of those rows said `gap` while the work
was built, committed and in several cases installed; the rows were stale, not
the code. Auditing them turned up a real packaging defect underneath:
`src/obs` was gated only on whether `libobs` happened to be findable, so
`gui-wm/qindaqt-desktop` built and installed
`/usr/lib64/obs-plugins/obs-qindaqt.so` when built on a host with OBS and
omitted it otherwise — the same ebuild producing different file lists per build
host, and a guaranteed file collision the first time anyone merged
`media-plugins/obs-qindaqt`. That is why the plugin package is installed
nowhere: it could not be.

`QINDAQT_BUILD_OBS_BRIDGE` now makes the ownership explicit. It defaults ON, so
repository builds and the standalone package are unchanged; the desktop ebuild
passes OFF.

**Resolved 2026-09-21.** `qindaqt-desktop-0.1.0_pre20260921-r1` ships no
obs-plugins file, and `media-plugins/obs-qindaqt-0.1.0_pre20260921-r2` installs
the module as its only file. Getting there took one more fix of the same shape:
the plugin's standalone build also installed the audio client and protocol it
merely links, so Portage refused the merge on ten file collisions with the
desktop package. Those installs are now guarded on
`QINDAQT_OBS_BRIDGE_STANDALONE`; a staged `cmake --install` of the standalone
tree yields exactly one file.

## 3. File manager

| Item | Status |
|---|---|
| Applications place, launch replaces the docked file manager ([ADR-0172](../adr/0172-applications-is-a-place-and-a-docked-window-can-replace-itself.md)) | committed |
| New-split gesture, mouse and keyboard, opening on the Applications tab | gap |

## 4. System tray

| Item | Status |
|---|---|
| StatusNotifier host registration so conformant items appear ([ADR-0166](../adr/0166-announce-a-status-notifier-host.md)) | committed |
| XEmbed to StatusNotifier proxy for Wine / Proton tray icons ([ADR-0229](../adr/0229-proxy-the-xembed-tray-into-status-notifier-items.md)) | committed, **not yet on a live session** — needs the next desktop revision; the startup bug below is fixed |

**The proxy shipped and nothing started it (2026-09-21).** Its systemd user
unit was `WantedBy=graphical-session.target`, and QindaQt never activates that
target — the same reason PowerDevil and KGlobalAccel are supervised children
rather than units. So systemd started the proxy never, `_NET_SYSTEM_TRAY_S0`
stayed unowned on the session's XWayland display, and a Wine, Proton or Steam
tray icon had nowhere to dock. Verified on the live session: `xprop -root
_NET_SYSTEM_TRAY_S0` reported no such atom, and `graphical-session.target` was
inactive.

The session supervisor now owns it (`xembedTrayProxyExecutable`), and the unit
keeps no `[Install]` section so there is exactly one automatic owner —
ADR-0229's own policy is that two trays fighting over one selection is worse
than one missing icon. An audit of every unit in the tree found this to be the
only case: the portal names `graphical-session.target` too, but it is D-Bus
activated and starts anyway, while the proxy owns an X selection rather than a
bus name and so had no fallback.


## 5. Rolled-up container names

| Item | Status |
|---|---|
| A user-set name stays on the rolled-up badge ([ADR-0168](../adr/0168-a-generated-name-never-displaces-a-real-title.md)) | committed |
| The badge is wide enough to read the page title, and follows it as it changes ([ADR-0189](../adr/0189-size-the-rolled-up-badge-to-its-label.md)) | committed |
| Names persist across compositor restarts | gap — blocked on a persistence owner for container identity: the appearance store is process-local by contract, nothing saves or restores live container topology at compositor start, and the explicit saved-workspace path adopts with a fresh container ID, so a name has nothing stable to attach to |

## 6. Wine / Proton window identity

| Item | Status |
|---|---|
| Program behind an opaque window class reported with a real name and icon ([ADR-0169](../adr/0169-report-the-program-behind-an-opaque-window-class.md)) | committed |
| Steam `appmanifest` names for `steam_app_<id>` windows ([ADR-0230](../adr/0230-name-and-picture-the-game-behind-a-launcher-class.md)) | committed |
| Icons extracted from the PE executable ([ADR-0230](../adr/0230-name-and-picture-the-game-behind-a-launcher-class.md)) | committed |

Both landed with the bounded KeyValues and PE readers described in ADR-0230 and
are on main; they reach a machine with the `pre20260921-r1` revision. The rows
read `gap` until 2026-09-21 because the ADR itself had never been written — the
code cited it from nineteen files while the decision record was missing, which
is also what made the work easy to overlook here.
