# Owner to-do (session 2026-09-28)

1. **Permanent fix: PipeWire wedged after re-login → dictation records nothing.**
   Evidence 17:3x: pipewire (started 16:50 by the new user manager) idle in ep_poll with 133 fds; every
   new client (pw-cli, wpctl) timed out; `pw.resource: client error -32 (Broken pipe)` at the 16:54 logout.
   Quick fix applied: restart pipewire/wireplumber/pipewire-pulse, qindaqt-audio-service, gabbee-service.
   Permanent: (a) root-cause with debug tools installed through Portage (strace/gdb); (b) session
   supervisor probes PipeWire at login with a bounded client call and restarts the stack when it does not
   answer; (c) Gabbee watchdog must detect "capture produced silence / no frames", not only a missing
   provider; (d) Audio1 reconnects after a PipeWire restart.
2. **KWin tablet proximity bug (upstream):** a pen entering proximity over a window decoration never
   sends proximity_in to the client (DecorationEventFilter consumes it; setCurrentSurface needs
   m_lastTablet), so apps drop pen input until the pen re-enters over them. Carry a patch in QindaQt's
   own KWin.
3. **Plasma-free QindaQt and QindaQt's own KWin** (owner request). Inventory done; plan pending approval.
4. File manager SIGSEGV (coredumps 15:48, 15:48, 15:56, 15:56) and the flaky
   qindaqt.file-manager-applications-ui segfault under parallel ctest.
5. Settings QML warnings in the session log (SettingsRouteHost/SettingsRouteSupplementalLoaders
   "Unable to assign [undefined] to bool", SettingsUnavailableRoute TypeError).
6. Pre-existing failing tests: controls-visual x14 baseline drift, settings-audio x3,
   shell-preference-values (12 vs 11 keys), shell-runtime-token-publication fixture (missing
   appearance.iconTheme), install tests writing /etc/xdg/autostart regardless of prefix.
7. Lane caveats: portal accent from chosen theme not its twin; container names lost on compositor
   restart; stored containerTitleDoubleClick=none overrides Corner Bar roll-up; no lift animation on
   touch pick-up; no Network desktop icon.
8. Delivery: after owner confirms, add the desktop revision to metadata/qinda-delivery and install on
   qinda.
9. **Per-display and per-virtual-desktop wallpapers** (owner request): a wallpaper choice per output and
   per desktop, with "same everywhere" as the default; Settings UI, Settings1 schema, shell wallpaper
   surfaces, and the wallpaper wiki/ADR-0078/0279.
10. **Ship the owner's added wallpapers as defaults** (owner request), after confirming provenance:
    owner-made AI images in ~/Pictures/Wallpapers and ~/Pictures/Wallpaper (landscape ones only; skip
    the screenshot, portrait and square images); KDE-store packs declare CC-BY-SA-4.0 (Scratchy,
    Scratchy-V2, Sky Blossom by jomada: ship with attribution); MacSequoia repackages Apple artwork and
    Spirited Away is Ghibli-derived: recommend not shipping; unknown origin: hell.png, image-7.png,
    5d7a65f8-...-Untitled-1-edit.png, Bul1/Bul2, Samurai Hacker.
11. **Tablet** (owner request, lane T running): desk-tablet rotation (Bamboo reports supportsRotation=false,
    so KWin `orientation` via calibration matrix), pen-display vs desk-tablet detection, pen displays
    follow their screen's rotation (KWin already applies the mapped output's transform), desk tablets
    compensate for a rotated monitor, and an input-area/output-area rectangle editor.
12. Display rotation already exists (Settings → Displays → Transform); confirm it reaches pen displays.
13. Wallpapers decided by the owner: ADD the five imported penguin-and-duck images to the existing bundled set (laptop
    ~/Pictures/Wallpapers image-gen-1..5), keeping every existing bundled wallpaper. Sent to lane W.
14. DONE (play.py 120 s timeout). codex-say playback serializes clips behind one lock; a player frozen on a dead PipeWire blocked every
    later clip. Part of item 1's fix: after an audio-stack restart, stuck players must be cleared (or the
    player should time out).

Status 2026-09-28 (final-build batch, all unbuilt, on fix/pen-eraser-chords):
- DONE in code: localizer (built), pen chords + device ownership + touch leak (built), stale settings
  retirement (built), Copperplate/Kith icons (QindaThemes ec9320a), file manager + QindaLutris icon
  thread races, Settings route warnings, Audio tab keys, OBS autostart install prefix, PipeWire
  liveness at login, KWin patch 0002 (ADR-0287).
- Waiting: lane T (tablet), lane W (wallpapers + the two shell fixture tests).
- Delivery rule (owner): no interim packages or laptop installs. Merge lanes to hub main as they land; package and install ONCE when all work is done and the owner is ready to log out.
  (kwin r2 with patch 0002, qindaqt-desktop pinned to the final commit, qinda-icons r2 already built).

## NEXT GAME (after this round's final build and delivery) — owner-reported
15. **VBAN never sends or receives**: qindaqt-audio-service.service.in hardens with
    RestrictAddressFamilies=AF_UNIX (+ SystemCallErrorNumber=EPERM), which blocks the AF_INET UDP
    socket VBAN (ADR-0185) needs. WirePlumberWorker::applyVbanOnWorker
    (src/services/audio_service/src/wireplumber_worker_vban.cpp:79-109) drops a failed
    VbanSender::start() silently (not added to m_vbanRuns, no log, no surfaced error). Allow AF_INET
    (and AF_INET6 if VBAN supports it) narrowly, and surface start failures.
16. **Console extras flicker to empty**: AudioOperationCoordinator::acceptLevels()
    (src/services/audio_service/src/audio_operation_coordinator.cpp:437-452) runs on every meter
    update and assigns m_snapshot.console = m_console.console() without re-applying .presets,
    .macros, .recording, .vban (republishConsole() at ~526 does). Settings' "Other computers" tab and
    presets/macros/recording readouts snap back to empty. Share one "project console" step.
17. **Easy per-device latency adjustment** (owner request): a per-output/per-input latency offset
    (PipeWire node/device latency offset, e.g. Props latencyOffsetNsec) so devices with different delay
    (Bluetooth headphones, HDMI, USB interfaces, VBAN peers) line up; a simple slider or "measure"
    helper per device in Settings → Audio, persisted per device identity, through Audio1.
18. **Compact QindaTK UI for Settings → Audio** (owner request): denser, more compressed layout
    using QindaTK's compact controls (rows, strips, small tab strip), keeping keyboard and accessibility.
19. notification-live.scale-150: Display1 runtime exits WriterStartFailed (status 4) in the nested session at 1.5 scale; pre-existing (no display_runtime changes this round). Also flaky under load: shade-visibility electron, notification-live race; qinda-only env: touch-osk x2, obs-bridge x2.
20. **Mixer delay stage for true output alignment** (follow-up to ADR-0288): the latency offset only changes reported latency (lip sync, combine-stream). Lining up two speakers or a VBAN peer fed from the same mixer bus needs a per-output delay stage in the mixer.
