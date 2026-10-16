# QindaQt daily-driving stability findings — 2026-10-16 (qinda-top)

Read-only investigation. Nothing was changed, restarted or killed.
Installed: qindaqt-desktop R29 = R23 image from container-wm 1a5359e48 + Display1 927039197 + keyring/prompt
a9c9768f4 + shell 42ec5bfa8; qindaqt-kwin r6 = 24d0c6a6fe. origin/main = 1a5359e48 + 67 commits.
Paths: container-wm `src/...`; fork `kwin:` = qindaqt-kwin.git. C = confirmed by code + live evidence, P = plausible.

## Critical
1. Session bus memory runaway (C). dbus-daemon 1199: VmData 4.17 GB, RSS 1.77 GB, swap 2.33 GB, growing
   ~120 KB/s; system swap 14.9/16 GB. Stuck client `qindaqt-settings --page appearance` (pid 333162, since
   10-15 10:20) stopped draining its bus socket (91 KB unread; daemon queue full); session.conf allows 1 GB
   per connection; high-rate broadcasts (Audio1.Levels 20/s, WindowsChanged 9-14/s, Sleep1.Changed 8/s) fill it.
   dbus.service OOMPolicy=stop and its cgroup also holds qindaqt-settings-service, night-light, ibus-portal,
   gabbee-service -> any OOM kill there stops the session bus (happened 10-02 12:21, shell then crashed).
   Why Settings stopped dispatching: unknown.
2. Lock/sleep wiring permanently dead after one slow bus reply (C). src/.../compositor_attachment.cpp:28,38-62
   polls every 100 ms with ~7 blocking calls (250 ms timeout); any failure -> clear(), never re-attached
   (native_lock_composition.cpp:86-131). Result: lock state Unknown (native_lock_state_monitor.cpp:163-169),
   ScreenSaver.GetActive errors (lock_objects.cpp:143-151), CanSuspend=false (sleep_coordinator.cpp:49-55),
   idle/manual lock refused (native_lock_runtime.cpp:198-210, qt_native_lock_request.cpp:127-134). Last lock
   10-15 08:15:35. logind_sleep_transport.cpp:31-33 lifetime timer calls revoke() every 250 ms -> unconditional
   emits (sleep_coordinator.cpp:111-118,128) = 8/s Sleep1.Changed; revoke also drops PrepareForSleep/Lock/Unlock
   subscriptions and the delay inhibitor -> no lock-before-sleep; lid suspends happen unlocked
   (lock.onResume is off in owner settings).
3. Battery death from s2idle wake loop (C). 4 of 6 forced power-offs (10-09 23:37, 10-10 09:38, 10-14 04:43,
   10-14 16:53) at 0.45-0.48 % discharging (UPower history). s2idle wakes within seconds ("amd_pmc: Last suspend
   didn't reach deepest state"); lid closed -> logind re-suspends ~31 s; 878 suspend entries in boot -1, 147 in -3.
   UPower CriticalPowerAction=HybridSleep at 2 % fails ("Cannot allocate memory", swap full) -> flat battery.
   Only s2idle exists (mem_sleep). Wake source unknown.
4. Suspend hangs with network mounts (P, strong). 10-12 07:46 and 10-14 08:49 (both on AC): "user@1000.service
   frozen-by-parent", no "PM: suspend entry", power key, hard off. systemd 261 freezes user sessions; File Manager
   network locations write sshfs user units (src/apps/file_manager/network/mount_unit.cpp:89-130) whose daemons
   run inside user@1000.service; Wi-Fi drops ~1 s before the freeze; nothing unmounts/opts out.
5. Compositor follows app-entry paths into the sshfs automount (C). kwin: src/utils/serviceutils.h
   fetchProcessServiceField(): KApplicationTrader::query over all apps + QFileInfo(exec).canonicalFilePath() on
   the main thread (permission checks, src/wayland_server.cpp:215-221). ~/.local/share/applications/
   origin-of-species.desktop Exec=~/tmp-wow-bits/... ; ~/tmp-wow-bits -> ~/Network/Void/... (symlink 10-12 22:19).
   qindaqt-kwin triggered the Void automount 184 times, all since 10-13 (58/37/66/23 per day). Each mount ~1 s
   of blocked compositor; unreachable qinda -> up to mount-timeout 20 s.

## High
6. Title-spinner re-sync storm (C). Spinner titles in QQTerm (Claude Code/Codex) -> kwin managedwindowregistry
   .cpp:113,145-159 (no rate limit) -> WindowsChanged (no production subscriber, kwincontrolendpoint.cpp:111),
   synchronizeChrome on every change (kwinhybridwindowactions.cpp:325) -> setRenderPlan without equality check
   (kwinchromemanager.cpp:255-262) -> full-frame chrome image repaint + texture upload (kwinscenechromeoverlay
   .cpp:186-205, imageitem.cpp:45-62; ~8 MB/tick for maximized QQTerm container); task facts resample, double
   JSON encode, blocking GetConnectionCredentials (kwinshelltaskfacts.cpp:174-179,221-258, kwinshellwindowactions
   .cpp:50,309,343). Shell: uncached icon disk lookups per row (icon_theme_locator.cpp:72-114,397-419),
   generationRevision in every row (task_list_applet_projection.cpp:39) defeats keyed diff (keyed_row_model.cpp:97),
   CommandSearchController::rebuild with palette closed (command_search_controller.cpp:88-92,
   launcher_applet_controller.cpp:238-250). Measured: kwin ~33-42 %, shell ~29-44 % of a core at idle.
7. Lock screen death = permanent black screen (C no relaunch, P triggers). kwin lockerlauncher.cpp:28-30,
   sessionlockcontroller.cpp:166-181; session never re-requests while Locked (native_lock_runtime.cpp:211-214).
   Exit paths: QML/role failure exit 2 (native_windows.cpp:15-25), 0x0 configure qFatal
   (shell_integration.cpp:160-161), buffer-size protocol error on output change (sessionlock_v1.cpp:146).
   Recovery: busctl --user call org.qindaqt.KWin /org/qindaqt/KWin/NativeLock org.qindaqt.KWin.NativeLock1 RequestLock
8. Screen capture portal dead all session (C symptom, P trigger). ScreenCast AvailableSourceTypes=0;
   ...qindaqt.capture unowned; OBS "pipewire-screen-capture-source not found". kwin capture-authority/
   captureauthority.cpp:120-161: m_restartCount never reset, 3 launches per compositor lifetime; backend
   main.cpp:23-31 exits 2 silently; session supervisor restarts xdg-desktop-portal 3.5 s after it starts
   (resident_service_refresh.cpp:83-90; list also has Audio1, Clipboard1, Display1 and Plasma's
   plasma-xdg-desktop-portal-kde.service).
9. Visibility snapshot all-or-nothing (C). One off-output or "active while hidden" window voids the batch
   (shellvisibilitysnapshot.cpp:204-266) -> task facts fail (kwinshelltaskfacts.cpp:258) -> dock Degraded,
   buttons disabled (TaskListEntryButton.qml:116), window actions "stale-generation" (shellwindowactions.cpp:188-195).
   Window stranded at 2560x1408+1536+32 after HDMI unplug (fork re-applies saved frames, kwinhybridinteraction.cpp:135-165, P).
10. Saved theme ignored at login (C). src/shell/common/catalogpaths.cpp resolveCatalogDataDirectory uses
    QStandardPaths::locate (first dir) -> ~/.local/share/qindaqt/themes (19 stale symlinks from 09-22) shadows
    /usr/share/qindaqt/themes (28, incl. qinda-corner-violet-dark). Settings uses locateAll. Warning at
    shellruntimeapplication.cpp:268 prints the requested theme, not the fallback.
11. Lock screen goes inert after 90 s (C). PAM attempt starts at lock with 90 s deadline (worker_process.h:47,
    worker_main.cpp:37); afterwards field hidden/disabled, Enter does nothing until Unlock is clicked
    (Main.qml:10-21,53-58,73; authentication_controller.cpp:109-126). Each timeout counts toward pam_faillock.

## Medium
12. Polling: shell serviceOwner() blocking 250 ms, ~13x per Sleep1.Changed (session_actions_internal.h:15-24 and
    session_actions_*); keyring 100 ms timer x5 blocking calls (compositor_attachment.cpp:38-61) + 250 ms
    (keyring/app/main.cpp:79-85); kwin displaypowerauthority.cpp:49-57,106-125 3 blocking calls/250 ms on main
    thread; Audio1 meters 50 ms, every client subscribes (qt_audio_transport.cpp:185-200), shell decodes to nobody
    (audio_client.cpp:39-95); OBS client retries 200 ms no backoff. polkitd 13 % CPU (P: CanReboot/CanPowerOff).
13. Compositor main-thread hangs x6 (P): blocking bus calls with default 25 s timeout to a swapped daemon; locker/
    capture teardown waits 1 s (lockerlauncher.cpp:34-39, capturechild.cpp:258-261); Wine identity /proc+file I/O
    (kwinshelltaskfacts.cpp:319-334, wineidentitycache.cpp:115-200); automount (#5).
14. Notifications fall to GNOME notification-daemon (C): x11-misc/notification-daemon owns the activation file;
    segfaulted 10-02 x3 (lost "new package installed" notice) and 10-14. QindaQt host is not activatable.
15. Settings aborts x10 (incl. 10-10, 10-11 current build) (P): settings_center/main.cpp declares the
    QQmlApplicationEngine before every model passed via setInitialProperties -> models destroyed first at exit.
    `--help` exits inside QCommandLineParser::process with live QApplication (also qindanote, qindafox-launcher).
16. Core dumps disabled session-wide (C): kwin src/main_wayland.cpp:327 -> protectLockAuthorityProcess()
    (session-lock/lockerlaunchpolicy.cpp:49) sets RLIMIT_CORE {0,0}; hard limit inherited by every app.
17. ~54 GL contexts, 233 MB VRAM in shell (C mechanism): basic render loop (shell/runtime/main.cpp:12) + Window-type
    popups kept per tile (TaskListEntryButton.qml:157-168,380; DockItemTile.qml:104-109; PanelPopup.qml:152).
18. Power ownership (C): Power1 runs with native policies off (power_service/app/main.cpp:42-53); logind
    HandlePowerKey=poweroff (a short press shuts down), HandleLidSwitch=suspend (QindaQt lid "none" ignored);
    hybrid sleep only from UPower critical action.
19. Display1 (C): "status 4" at all 8 logins (wl_display_connect before WAYLAND_DISPLAY is published,
    qt_wayland_output_management_port.cpp:76; activated by kwin -> NightLight); self-exit
    "sleep-without-delay-authority" on back-to-back suspends (qt_session_safety_port.cpp:169). Gabbee drop-ins
    90-gabbee-recovery.conf StartLimitBurst=2/60 s on QindaQt units; watchdog restarted Display1 10-15 10:12.
20. Login races: portal backend "native composition unavailable" x2, NightLight "startup failed: " (empty) x2.
21. Stale container state (C): "no valid maximize area" x350 for dissolved hybrid-r7; forgetContainer missing on
    chrome drag (kwinhybridinteraction.cpp:294) and keyboard group/tile (kwinhybridwindowmanagement.cpp:271,350).
22. Portal: 195 "Request refused", 3 "Inhibitor unavailable" this boot (RequestRegistry::begin, interface unknown).

## Low
23. pam_qindaqt_keyring calls GetUnit with the service name as interface (keyring/pam/system_owner.cpp:13,90,110)
    -> rejected; keyring never unlocked from the lock screen. pam_unix "setuid failed" is debug noise.
24. Audio1 console data loops SCHED_OTHER (RestrictRealtime=true, no rtkit); 10 meters + VBAN send keep graph awake.
25. KWIN_FORCE_SW_CURSOR=1 in ~/.zprofile:3; stray autostarts (KDE xwaylandvideobridge, printer applet);
    two ibus-x11; decoration font reported as Liberation Serif.
26. Process: installed desktop is a patchwork of 4 source commits; physical suspend/unlock journeys never qualified.

## Open questions
s2idle wake source; why Settings stopped dispatching; when the session lost its compositor attachment
(between 10-15 07:53 and 20:43); what was stuck in the 10-12/10-14 freezes; which interface gets "Request refused".
