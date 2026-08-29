# Elara Finch handoff: smallest safe whole-desktop virtual Wayland qualification architecture (P0–P3)

- **Timestamp:** 2026-08-28T05:39:05Z
- **Identity:** Elara Finch, Display/output architecture analyst and exact reviewer — Anthropic `claude-fable-5`, reasoning maximum
- **State:** handoff — analysis/review only. No product edit, Git mutation, configure, build, test, runtime, GUI, input, or host-state change was performed. Durable writes: this reply, my claim `1787894445-elara-finch-virtual-desktop-qualification-architecture-claim.md`, and `ops/team/workers/elara-finch.md`.
- **Outcome served:** `docs/TASK_LIST.md:10-22` "Interactive virtual desktop integration" under ADR-0015 (`docs/wiki/adr/0015-qualify-function-before-resource-refinement.md:22-36,38-49`); board row QQ-007 stopping point (`ops/team/features.json:264-278`).
- **Evidence identity:** manager worktree `/home/cabewse/work_SPaC3/container-wm-workers/qst1-manager-integration`, exact HEAD `335a93b5ce740fca6f12e95a904f50c3d39e86c9`. Every `path:line` below was read from `git show 335a93b5:<path>` (the manager's staged, conflicted Controls integration in that tree was not used). Soren Pike's preserved notification-live worktree (`/home/cabewse/work_SPaC3/container-wm-workers/notification-live`, base `c4982697`, 70 uncommitted paths) was read only where marked **[candidate]**. Upstream anchors are the pinned KWin `v6.6.5` tag (`docs/wiki/architecture/compositor-session.md:17-20`, commit `b04d59c03749484a8a0ed5a8d4cda515a267c59b`), fetched from `https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/<path>`. Host facts are read-only observations of this reference machine on 2026-08-28.
- **Labelling:** **[verified]** = read in the repository, pinned upstream, or host; **[candidate]** = Soren's uncommitted tree; **[inference]** = reasoning from verified facts, must be confirmed by the first executable slice; **[proposal]** = design choice for the implementer/manager.

## 0. Summary

The whole-desktop gate is reachable with **zero product-source change for slices S0–S2** and two small seams for S3–S5, by composing components that already exist: the `qindaqt-wm → kwin_wayland --virtual --exit-with-session qindaqt-session → notification host + shell` production chain, D-Bus `Exec=` activation of Settings1/Audio1 from a staged install, the development-gated `InjectTestInput` `KWin::InputDevice`, KWin 6.6.5's own `org.kde.KWin.ScreenShot2` plugin, and Soren Pike's exact private-process-group teardown. Three facts decide the design and are not visible from the wiki:

1. **KWin 6.6.5 screenshots are OpenGL-only** (`src/plugins/screenshot/screenshot.cpp`: `dynamic_cast<EglBackend *>(Compositor::self()->backend())`, else `std::nullopt`) while **every existing nested row forces `KWIN_COMPOSE=Q`** (`tests/session/nested_session_scenario.py:176`, `tools/qindaqt_dev/backends.py:106`). Capture rows must run the virtual backend with OpenGL enforced (`KWIN_COMPOSE=O2`, fail-closed) on a render node; a QPainter-capable capture seam is the later CI-determinism item (P2-1). **[verified upstream + repository]**
2. **Every current nested row passes `--no-lockscreen --no-global-shortcuts`** (`tests/session/test_nested_session.py:215-222`, `test_shell_surface_nested.py:217-218`, `tools/qindaqt_dev/backends.py:97-98`). On this host `kwin_wayland` links `libKGlobalAccelD.so.0` and `libKScreenLocker.so.6` and the `kglobalacceld` package ships **no** D-Bus activation file (only `plasma-kglobalaccel.service`), so those flags remove the only KGlobalAccel and KSldApp providers a private session can have; ADR-0011's authenticated lock monitor then never reports `Unlocked` and notification presentation is denied. Whole-desktop rows must omit both flags and write a private `kscreenlockerrc` (Soren's pattern). **[verified host + candidate]**
3. **Existing runners cannot prove deterministic teardown**: `subprocess.run(..., timeout=…)` (`test_nested_session.py:223-230`, `test_shell_surface_nested.py:226-233`) kills only `dbus-run-session`; `/dev/uinput` is `root:input 0660` and this user is in `input`, so the `uinput` hazard documented at `docs/wiki/development/testing-harness.md:5-17` is real here. `bwrap 0.11.2` and unprivileged user namespaces are available (`/proc/sys/kernel/unprivileged_userns_clone=1`), which gives a kernel-guaranteed PID-namespace teardown plus a private `/dev` without `uinput`/`input`. **[verified host + repository]**

Counts: **3 P0, 6 P1, 6 P2, 6 P3** (Section 9). Recommended first executable slice: S1 (`desktop.virtual.boot.1080p`).

## 1. Verified facts that constrain the design

### 1.1 Repository (HEAD `335a93b5`)

- F1 Launcher contract: `qindaqt-wm` validates `--virtual/--windowed/--drm`, `--socket`, `--width/--height` (320x200…32768x32768), `--scale` (0.5…4.0), `--output-count` (1…16), `--no-xwayland`, `--no-lockscreen`, `--no-global-shortcuts`, `--replace`, `--test-scenario <path>`, `--session <path>`, `--plugin-root` (`src/session/sessioncommandline.cpp:21-63`; bounds `src/session/kwincommandbuilder.cpp:42-57`) and `execvp`s `kwin_wayland` (`src/session/main.cpp:34-37`) with `--exit-with-session <session>` (`kwincommandbuilder.cpp:98-100`). The production default session is `qindaqt-session` (`src/session/CMakeLists.txt:30`). The only development marker is `--test-scenario`, which sets `QINDAQT_TEST_SCENARIO` + `QINDAQT_DEVELOPMENT_CONTROL=1` after clearing inherited copies (`src/session/sessionenvironment.cpp:23-31`); `QT_PLUGIN_PATH` is prepended, never replaced (`:33-44`). The launcher does **not** unset `DISPLAY`/`WAYLAND_DISPLAY`/`WAYLAND_SOCKET`/`XAUTHORITY`.
- F2 Supervisor contract: `qindaqt-session` is KWin's direct child (parent witness + `PR_SET_PDEATHSIG` `src/session_supervisor/src/direct_parent_process.cpp:32-59`), starts exactly `qindaqt-notification-host --presentation-token-fd N` and `qindaqt-shell [--profile P] [--theme T] --compositor-pid <kwin> --presentation-token-fd N` (`src/session_supervisor/src/session_process_supervisor.cpp:28-46,70-98`; token fd `tokenized_process_launcher.cpp:52-66`), and ends the session when either child exits (`:129-149`). It starts **no** Settings1/Audio1 (those are D-Bus activated). It has **no** SIGTERM→`quit()` bridge (`src/session_supervisor/app/main.cpp:14-71`).
- F3 Shell/host options: `--profile`, `--theme`, `--profile-dir`, `--theme-dir`, `--applet-dir`, `--applet-policy`, `--presentation-token-fd`, `--compositor-pid`, `--list` (`src/shell/runtime/runtimeoptions.cpp:20-45`); host `--presentation-token-fd` (`src/services/notification_host/app/main.cpp:78-81`).
- F4 Activation descriptors are plain `Exec=` files: `src/services/settings_service/data/org.qindaqt.Settings1.service.in:1-3`, `src/services/audio_service/data/org.qindaqt.Audio1.service.in:1-4` (its `SystemdService=` is ignored by a `dbus-run-session` daemon). Installed to `share/dbus-1/services` (`src/services/settings_service/CMakeLists.txt:50`, `src/services/audio_service/CMakeLists.txt:74`). The private-activation pattern already exists: a fixture `share/dbus-1/services` on `XDG_DATA_DIRS` (`tests/services/audio_client/tst_audio_activation.cpp:62-77`).
- F5 Development input seam: gate `mutationsEnabledForSession` requires both markers (`src/compositor/kwin/mutationcontrol.cpp:8-20`); the injector is constructed only when enabled (`src/compositor/kwin/qindaqtkwinplugin.cpp:36,44-53`), registers one combined keyboard+pointer `KWin::InputDevice` named "QindaQt Development Input" through `InputRedirection::addInputDevice` (`kwindevelopmentinputinjector.cpp:17-42,83-87`) and emits `pointerMotionAbsolute`/`keyChanged`/`pointerButtonChanged`/`pointerFrame` (`:100-122`). Vocabulary at HEAD: `pointer-absolute`, buttons Left/Right, keys **LeftMeta, LeftShift, Down, Enter only**; ≤64 events per batch (`developmentinputprotocol.h:14-30,52-63`). Service `org.qindaqt.Compositor` at `/org/qindaqt/Compositor` (`qindaqtkwinplugin.cpp:30-31,97`); methods incl. `Capabilities`, `Outputs`, `InputCapabilities`, `Windows`, `InjectTestInput`, `ReinitializeCompositingForTest` (`kwincontrolendpoint.h:64-86`; `compositor/dbus/org.qindaqt.Compositor1.xml:4-56`). Client-side driver: `tests/session/hybridtestinputdriver.h:47-72`, `.cpp:293-319` (checks `status: injected`, `eventCount`, stable `deviceId`).
- F6 Existing nested boot pattern: `isolated_environment()` strips `DBUS_SESSION_BUS_ADDRESS`, `DISPLAY`, `WAYLAND_DISPLAY`, the development markers and `QINDAQT_DOTOOL`, creates private XDG dirs, and sets `KWIN_COMPOSE=Q`, `QT_QPA_PLATFORM=wayland`, `QT_QUICK_BACKEND=software` (`tests/session/nested_session_scenario.py:146-181`); `write_virtual_output_config()` pins scale and places outputs at `x = index * logical_width` (`:184-221`); representability requires one common mode/scale, no transform, integral logical extents (`:70-77,109-135`). Launch = `dbus-run-session -- qindaqt-wm --plugin-root … --virtual --width --height --scale --output-count [--test-scenario] --no-lockscreen --no-global-shortcuts --session <probe>` (`test_nested_session.py:192-222`; `test_shell_surface_nested.py:204-221`). Results are one stdout JSON marker (`QINDAQT_PROBE=` `sessionprobe.cpp:201,222`; `QINDAQT_SHELL_SURFACE_PROBE=` `shellsurfaceprobe.cpp:328`). Registration and tool discovery: `tests/session/CMakeLists.txt:96-111,165-196,256-283,343-358,383-408`; staged install via `cmake --install <build> --prefix <stage> [--config]` (`tests/session/test_installed_plugin_discovery.py:38-56`).
- F7 Capture determinism pattern (preview only): `frameSwapped → grabWindow → exact-size assert → PNG` (`src/shell/app/screenshotcapture.cpp:26-74`) with `offscreen`/`software`/`basic`/`QT_SCALE_FACTOR=1` (`captureenvironment.cpp:29-45`); matrix rows 1080p/WUXGA/1440p/macOS-WUXGA (`tests/shell/tst_shell_capture.cpp:23-31`, `qindaqt.shell-capture-matrix` `tests/shell/CMakeLists.txt:41-58`). This is a client-side scene-graph grab, not a composed desktop.
- F8 Exact-process cleanup guard: revalidate `/proc/<pid>/exe` before TERM→KILL (`tests/services/settings_service/tst_settings_service_process_lifecycle.cpp:99-117`).
- F9 Scenario catalog (`tests/scenarios/`): representable single-output rows are `single-1080p` (qindaqt/qinda-dusk), `single-wuxga`, `single-1440p`, `single-1080p-125` (1536x864 logical; gnome-inspired/qinda-light), `single-1080p-150` (1280x720; mate-inspired/qinda-dark), `single-1440p-125` (2048x1152; unity-inspired/qinda-dusk); representable multi-output: `dual-1080p-horizontal` (2 × 1920x1080). **Not** representable by the current CLI: `single-1440p-150` (2560/1.5 non-integral), `single-wuxga-portrait` (transform), `mixed-dpi-1080p-1440p`, `portrait-wuxga-1440p`, `laptop-external-hotplug`, `dynamic-output-reconfigure`. Themes available: `data/themes/qinda-{light,dusk,dark,high-contrast,macos}.json`; ten stock profiles in `data/profiles/`.
- F10 Tool discovery hazard: `tests/session/CMakeLists.txt:96` uses `find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session)`; on this host `PATH` resolves `dbus-run-session`, `dbus-daemon`, `python3`, and `unshare` to `/home/linuxbrew/.linuxbrew/bin/*` ahead of `/usr/bin` (`busctl`, `bwrap`, `kwin_wayland`, `weston`, `Xwayland`, `kbuildsycoca6`, `kscreenlocker_greet`, `pipewire`, `wireplumber` are `/usr/*`). `mkdocs` and `smem` are absent.
- F11 CI: the GitHub production job strips `cap_sys_nice` from `kwin_wayland` inside a container and never grants `SYS_NICE` (`.github/workflows/ci.yml:134-155`); CI has no KWin plugin ABI lane (`QINDAQT_BUILD_KWIN_PLUGIN=OFF`, `:170-172`). Therefore the whole-desktop matrix is a **reference-machine gate first**, CI later.

### 1.2 Pinned KWin 6.6.5 (tag `v6.6.5`)

- U1 `src/plugins/screenshot/screenshotdbusinterface2.cpp`: service/object `org.kde.KWin.ScreenShot2` / `/org/kde/KWin/ScreenShot2`; `checkPermissions()` returns true when `KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1` (read once, static), else resolves the caller PID via `connection().interface()->servicePid(message().service())` and requires `KWin::fetchRestrictedDBusInterfacesFromPid(pid)` to contain `org.kde.KWin.ScreenShot2`, otherwise replies `org.kde.KWin.ScreenShot2.Error.NoAuthorized` ("The process is not authorized to take a screenshot"). Methods: `CaptureScreen(QString name, QVariantMap options, QDBusUnixFileDescriptor pipe)`, `CaptureActiveScreen`, `CaptureWorkspace`, `CaptureArea(x,y,w,h,…)`. Raw pixels are written to the pipe; the reply map carries `type:"raw"`, `format` (QImage::Format), `width`, `height`, `stride`, `scale`, plus `screen`/`windowId`. Options: `include-cursor`, `include-decoration`, `include-shadow` (default true), `native-resolution`, **`hide-caller-windows` (default true)**.
- U2 `src/plugins/screenshot/screenshot.cpp`: all `takeScreenShot()` paths require `EglBackend`; QPainter compositing yields no image. `src/plugins/screenshot/metadata.json`: `EnabledByDefault: true`; host ships `/usr/lib/qt6/plugins/kwin/plugins/screenshot.so`.
- U3 `src/utils/serviceutils.h`: `fetchRestrictedDBusInterfacesFromPid` derives the executable from the PID, finds a desktop file whose `Exec` first token's canonical path matches via `KApplicationTrader::query`, and reads `X-KDE-DBUS-Restricted-Interfaces`; no match → empty list.
- U4 `src/compositor.cpp`: `KWIN_COMPOSE` — `O2`/`O2ES` enforce OpenGL; any other value disables OpenGL; candidates OpenGL → QPainter → none; when `KWIN_COMPOSE` is set and the requested backend fails, KWin logs "Could not fulfill the requested compositing mode" and `qApp->quit()`; QPainter uses `outputBackend()->createQPainterBackend()`.
- U5 `src/main_wayland.cpp`: `--exit-with-session` runs the session command through `QProcess` (`KShell::splitArgs`, so arguments are allowed) with `processStartupEnvironment()`; KWin `exit()`s with the child's exit code (crash → -1). `--no-lockscreen` → `setSupportsLockScreen(false)`; `--no-global-shortcuts` → `setSupportsGlobalShortcuts(false)`. Virtual outputs are created as `OutputInfo{ .geometry = Rect(QPoint(), initialWindowSize), .scale }` for every index (same origin; placement comes from the output configuration store, which is why F6 writes `kwinoutputconfig.json`).
- U6 `src/backends/virtual/virtual_backend.cpp`: both `createQPainterBackend()` and `createOpenGLBackend()` exist; OpenGL needs a DRM render device (GBM); `createVirtualOutput(name, description, size, scale)` names connectors `Virtual-<name>`; **no frame-to-disk path exists** in 6.6.5.
- U7 `src/wayland_server.cpp`: KWin inserts `WAYLAND_DISPLAY=<socket name>` into `processStartupEnvironment()` and its own environment; children therefore always receive the nested socket name regardless of the inherited value.

### 1.3 Reference host (read-only, 2026-08-28)

- H1 `kwin 6.6.5-4`, `qt6-base 6.11.1-1`, `layer-shell-qt 6.6.5-2`; `getcap /usr/bin/kwin_wayland` = `cap_sys_nice=ep`; `kwin` depends on `kglobalacceld`, `kscreenlocker`, `libpipewire`, `pipewire-session-manager`; `ldd kwin_wayland` shows `libKGlobalAccelD.so.0` and `libKScreenLocker.so.6`.
- H2 GPU `amdgpu`; `/dev/dri/renderD128` is `crw-rw-rw- root:render` (no group needed); `/dev/dri/card1` is `root:video`.
- H3 `/dev/uinput` `crw-rw---- root:input`; this user is in `input`. `/dev/input/event0` exists.
- H4 `bwrap 0.11.2` (`/usr/bin/bwrap`), `unprivileged_userns_clone=1`, `user.max_user_namespaces=127374`; `busctl`, `Xwayland`, `weston`, `xdpyinfo`, `kbuildsycoca6`, `/usr/lib/kscreenlocker_greet`, `/usr/lib/kglobalacceld`, `pipewire`, `wireplumber` present. 24 logical CPUs.

## 2. Target architecture (smallest safe)

### 2.1 Process graph per row

```
host: ctest (RUN_SERIAL) → python3 tests/session/test_desktop_session_nested.py --outer
  ├─ stage once:  cmake --install <build> --prefix <run-root>/stage   (F6 pattern)
  ├─ run root:    <build>/tests/session/desktop-session-runs/<run-id>/{stage,artifacts,logs}
  ├─ sandbox:     bwrap --unshare-pid --unshare-net --die-with-parent --new-session
  │                 --dev-bind / /  --proc /proc  --dev /dev  --dev-bind /dev/dri/renderD128 /dev/dri/renderD128  (capture rows only)
  │                 --tmpfs /run/user/<uid>  --tmpfs /tmp  --perms 1777 --dir /tmp/.X11-unix  --perms 0700 --dir /tmp/qq/rt
  │                 --bind <run-root> <run-root>  --clearenv --setenv …(allow-list)…
  │                 -- /usr/bin/dbus-run-session -- /usr/bin/python3 tests/session/test_desktop_session_nested.py --inner …
  │    [PID ns] dbus-daemon (private bus; XDG_DATA_DIRS=<stage>/share:/usr/share)
  │      └─ inner driver
  │          ├─ qindaqt-settings-service        (started explicitly, awaited via busctl; Soren's pattern)      [resident]
  │          ├─ qindaqt-wm --plugin-root <stage plugins> --virtual --width W --height H --scale S --output-count N
  │          │            --test-scenario <scenario.json> --session "qindaqt-session --profile P --theme T"
  │          │   = kwin_wayland (QindaQt plugin + development input device, in-process KGlobalAccelD + KSldApp, rootless Xwayland)
  │          │      └─ qindaqt-session → qindaqt-notification-host + qindaqt-shell                             [resident]
  │          ├─ org.qindaqt.Audio1 activated on first call (stage share/dbus-1/services)                        [resident]
  │          ├─ qindaqt-settings (first-party app) + qindaqt-desktop-session-probe (Qt Wayland client: painted windows,
  │          │   DevelopmentInputDriver, ScreenShot2 client, inventory/evidence)                                  [apps]
  │          └─ measurement: namespaced /proc → smaps_rollup Pss + stat utime/stime over the idle window
  └─ host audit after bwrap exit: no /proc/*/environ with QINDAQT_SESSION_RUN_ID=<run-id>; artifacts + evidence JSON present
```

**Why this shape [proposal]:** the private PID namespace makes "everything the desktop started is gone" a kernel fact (`--die-with-parent` SIGKILLs the whole sandbox when bwrap or its parent dies; the PID-1 reaper dies with the namespace); the private `/dev` contains no `uinput`/`input` nodes, so a test bug cannot reach the host seat even with this user in `input`; `--tmpfs /run/user/<uid>` removes the host Wayland, X, session-bus, and PipeWire sockets from view; `--unshare-net` removes abstract-socket reachability (`@/tmp/.X11-unix/X0`, abstract D-Bus). Containment is therefore structural and auditable rather than a runtime observation. `XDG_RUNTIME_DIR` must stay short (`/tmp/qq/rt`, sandbox-private) because `sun_path` is limited to 108 bytes; a runtime dir under the worktree's build path would exceed it **[verified: path length; inference: bwrap `--dir` mode flags]**.

**Fallback mode [proposal]:** where `bwrap`/user namespaces are unavailable (CI containers), the same driver runs Soren's `run_private_process_group` (**[candidate]** `notification_live_process.py:110-146`, validated pgid `:30-43`, bounded TERM→KILL with survival assertion `:74-107`) with the identical environment allow-list; the row records `containment.mode = "process-group"` and must still assert `QINDAQT_DOTOOL` unset and no `dotool` on `PATH`. Selecting the fallback is an explicit CMake choice (`-DQINDAQT_DESKTOP_CONTAINMENT=process-group`), never automatic.

### 2.2 Environment allow-list inside the sandbox

`HOME`, `XDG_{CONFIG,DATA,CACHE,STATE}_HOME` (private), `XDG_RUNTIME_DIR=/tmp/qq/rt`, `XDG_DATA_DIRS=<stage>/share:/usr/share`, `XDG_CURRENT_DESKTOP=QindaQt`, `XDG_SESSION_DESKTOP=qindaqt`, `XDG_SESSION_TYPE=wayland`, `PATH=<stage>/bin:/usr/bin`, `LANG=LC_ALL=C.UTF-8`, `TZ=UTC`, `QT_QPA_PLATFORM=wayland`, `QT_QUICK_BACKEND=software`, `KWIN_COMPOSE=Q` (functional rows) or `KWIN_COMPOSE=O2` (capture rows), `KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1` (capture rows only, see 2.4), `QINDAQT_SESSION_RUN_ID=<run-id>`. Explicitly absent: `DISPLAY`, `WAYLAND_DISPLAY`, `WAYLAND_SOCKET`, `XAUTHORITY`, `DBUS_SESSION_BUS_ADDRESS` (set by `dbus-run-session`), `QINDAQT_DEVELOPMENT_CONTROL`/`QINDAQT_TEST_SCENARIO` (set by the launcher only), `QINDAQT_DOTOOL`, `QINDAQT_ALLOW_HOST_UINPUT`, `LD_*`. Use `--clearenv` + `--setenv` so the allow-list is the whole environment.

### 2.3 Input confinement contract

- The nested seat has no libinput devices; the only device is the development `KWin::InputDevice` (F5). The probe asserts `InputCapabilities()` lists exactly one device named "QindaQt Development Input" and `Capabilities().controlMode == "development-test"`, and every `InjectTestInput` reply returns the same `deviceId` (`hybridtestinputdriver.cpp:293-319`).
- Structural host proof recorded in evidence: `/dev/uinput` and `/dev/input` absent inside the sandbox, `/run/user/<uid>` empty, `DISPLAY`/`WAYLAND_DISPLAY` absent in the inner environment (Soren's refusal check, **[candidate]** `test_notification_live_nested.py:161-168`), network namespace private. The "host pointer did not move" clause of TASK_LIST is satisfied by impossibility, not by polling the host (polling would itself require connecting to the host compositor).
- Screenshots with `include-cursor: true` after a pointer move show the nested cursor at the injected coordinate **[inference]** — useful reviewer evidence, not the containment proof.

### 2.4 Screenshot contract

- Capture rows run `KWIN_COMPOSE=O2` with `/dev/dri/renderD128` bound; the probe asserts `Capabilities`/KWin report OpenGL compositing (or `CaptureScreen` succeeds) so a silent QPainter fallback cannot produce a passing row without images (U4 makes KWin quit instead).
- Authorization: set `KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1` in the sandbox environment for capture rows (U1). This is safe because the bus is private to the sandbox and the compositor control bus already has no caller authentication in production (`compositor-session.md:242-243`). The production-faithful alternative — a desktop file in the private `XDG_DATA_HOME/applications` with `Exec=<probe path>` and `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2` (U3) — needs a ksycoca build inside the sandbox **[inference]** and is deferred to P3.
- Call `CaptureScreen("Virtual-<i>", {"native-resolution": true, "include-cursor": true, "hide-caller-windows": false}, pipe)` per output; read `width*height*stride` bytes, construct `QImage(format)`, save PNG + JSON sidecar `{output, phase, width, height, scale, format, sha256}`. Assert pixel size = logical × scale (e.g. 1536x864@1.25 → 1920x1080). `hide-caller-windows` must be `false` or the probe's own painted windows vanish (U1).
- The read-only production row (no `--test-scenario`, no bypass env) asserts `InjectTestInput → control-disabled` (existing `compositor.production-control-read-only` semantics) **and** `CaptureScreen → org.kde.KWin.ScreenShot2.Error.NoAuthorized`, proving the bypass is confined to development rows.

### 2.5 Memory and CPU contract

- Inside the PID namespace, `/proc/[0-9]*` **is** the desktop process set. After boot settles (all awaited services + shell surfaces mapped + 10 s), sample three times at 5 s spacing: aggregate `Pss:` from `/proc/<pid>/smaps_rollup` (kB) excluding the driver and probe; per-process rows keyed by `comm` and `/proc/<pid>/exe`. Report median aggregate against the 1,024 MiB ceiling (ADR-0015) as a **measurement**, fail only above the ceiling.
- Idle CPU: Σ(utime+stime) delta over a 30 s window / (30 × `CLK_TCK`) → percent of one core; report against the 1 % target (reference machine only).
- Expected composition **[inference, not evidence]**: kwin_wayland, Xwayland, dbus-daemon, qindaqt-session, qindaqt-notification-host, qindaqt-shell, qindaqt-settings-service, qindaqt-audio-service (in `pipewire-unavailable` state — truthful; harness `:665-670`), qindaqt-settings, probe windows.

### 2.6 Deterministic teardown contract

1. Inner driver: `SIGTERM` → `kwin_wayland`; KWin quits and destroys the session `QProcess`; `qindaqt-session` and its children die by `PR_SET_PDEATHSIG SIGKILL` (F2). Record compositor exit code (expected 0 on graceful quit **[inference]**; a `-1/255` means the session child crashed first).
2. Inner driver waits ≤5 s for `kwin_wayland`, then exits; `dbus-run-session` exits; bwrap (PID 1 of the namespace) exits → kernel kills any survivor.
3. Outer: wait bwrap ≤ 10 s (TERM→KILL by process group as backstop), then audit: no `/proc/*/environ` containing `QINDAQT_SESSION_RUN_ID=<run-id>`, evidence JSON present, `artifacts/` contains logs (`kwin.log`, `session.log`, `settings-service.log`), captures, and `measure.json`.
4. Every failure path still runs step 3; the row fails closed on any survivor or missing artifact.

## 3. Reusable components (exact)

| Need | Reuse | Anchor |
| --- | --- | --- |
| Launch KWin virtual with plugin and session | `qindaqt-wm` | F1 |
| Production session composition | `qindaqt-session` + host + shell | F2, F3 |
| Private XDG/env, scale pinning, representability | `nested_session_scenario.py` | `:146-181,184-221,70-77,109-135` |
| Staged install of every binary/plugin/descriptor | `test_installed_plugin_discovery.py` | `:38-56`; **[candidate]** `NotificationLiveTests.cmake:76-106` argument shape |
| Await service owners by PID | **[candidate]** `notification_live_process.py:149-170` (`busctl status`) | H4 `busctl` |
| Private-process-group teardown fallback | **[candidate]** `notification_live_process.py:30-146` | — |
| Private locker policy, no `--no-lockscreen` | **[candidate]** `test_notification_live_nested.py:150-158,286-306` | Section 0 (2) |
| Development input driver | `tests/session/hybridtestinputdriver.{h,cpp}` `DevelopmentInputDriver` | F5 |
| Compositor D-Bus probe client | `tests/session/compositorprobeclient.{h,cpp}` | `tests/session/CMakeLists.txt:112-138` |
| Painted Wayland test window | `PaintedMaximizedWindow` | `tests/session/shellsurfaceprobe.cpp:45-87` |
| Exact-exe cleanup guard | `tst_settings_service_process_lifecycle.cpp:99-117` | F8 |
| PNG write + exact-size assert | `screenshotcapture.cpp:40-74` (pattern only) | F7 |
| Private D-Bus activation | `tst_audio_activation.cpp:62-77` | F4 |
| Scenario catalog and theme/profile variants | `tests/scenarios/*.json`, `data/themes`, `data/profiles` | F9 |
| Registration style that keeps `tests/session/CMakeLists.txt` under source-shape limits | **[candidate]** `NotificationLiveTests.cmake` include | AGENTS.md 500/600-line rule |

## 4. Missing production seams and harness gaps

| Id | Gap | Smallest repair | Owner suggestion |
| --- | --- | --- | --- |
| G1 (P0) | No whole-session entry point (ADR-0015 consequence `:42-43`) | New `tests/session/test_desktop_session_nested.py` (outer/inner) + `desktop_session_sandbox.py` + `desktop_session_measure.py` + `qindaqt-desktop-session-probe` (C++, reuses F5/F7/F8) + `DesktopSessionTests.cmake` | Harness implementer (Soren Pike after his candidate lands, or a deliberate hire) |
| G2 (P0) | Screenshot impossible under `KWIN_COMPOSE=Q` | Capture rows enforce `O2` + render-node bind; later P2-1 seam | Harness now; compositor owner later |
| G3 (P0) | No kernel-enforced teardown/containment | `bwrap` sandbox builder with explicit fallback | Harness implementer; ADR (P2-6) |
| G4 (P1) | Input vocabulary (4 keys) | Integrate Soren's `DevelopmentInputKey` +N/Tab/Escape/Space (**[candidate]** diff on `developmentinputprotocol.h`), keep `MaxEvents 64`; later bounded evdev allow-list | Compositor plugin owner via Soren's candidate |
| G5 (P1) | `--no-lockscreen`/`--no-global-shortcuts` in all nested rows | Whole-desktop rows omit both; private `kscreenlockerrc` `RequirePassword=false` | Harness |
| G6 (P1) | `qindaqt-session` has no SIGTERM→quit bridge (crash-shaped end) | ~15 lines: `signal(SIGTERM/SIGINT)` → self-pipe/`QSocketNotifier` → `QCoreApplication::quit()` in `src/session_supervisor/app/main.cpp` (supervisor `stop()` already TERM→KILLs children `:151-161`) | Session supervisor owner; coordinate with Soren's supervisor diff |
| G7 (P1) | `find_program` picks linuxbrew tools | Pin `/usr/bin/{dbus-run-session,python3,bwrap,busctl}` for desktop rows (`find_program(... PATHS /usr/bin NO_DEFAULT_PATH)` or cache vars); record resolved paths in evidence | Harness; note for all existing rows |
| G8 (P1) | ScreenShot2 hides caller windows by default; cursor excluded | Pass `hide-caller-windows:false`, `include-cursor:true` | Probe |
| G9 (P1) | Unrepresentable scenarios (F9) | Matrix uses representable rows only; heterogeneous/rotated rows wait for D0's `AddVirtualOutputForTest` seam (Rhea) | Manager routing |
| G10 (P2) | QPainter-capable, CI-deterministic capture | Development-gated `CaptureOutputForTest(name)` in the QindaQt plugin rendering the scene to a `QImage` `RenderTarget` (private KWin API, exact 6.6.5 ABI) | Compositor plugin owner; ADR |
| G11 (P2) | Launcher/dev tool env hygiene | `SessionEnvironment::apply` unsets `DISPLAY`/`WAYLAND_DISPLAY`/`WAYLAND_SOCKET`/`XAUTHORITY` for `--virtual` (KWin re-exports, U7); `tools/qindaqt_dev/isolation.py:37-45` strips the same | Session launcher owner |
| G12 (P2) | Audio1 without PipeWire in-desktop | Private PipeWire+WirePlumber inside the sandbox (fixture pattern of `qindaqt.audio-wireplumber-runtime`) | Audio owner, later slice |
| G13 (P2) | Multi-output shell publication unclaimed (harness `:240-242`) | Dual-1080p row asserts boot + per-output captures only | QQ-004.09 scope |
| G14 (P2) | Sandbox dependency and compositing policy undocumented | ADR "Contain nested desktop qualification in an unprivileged sandbox; enforce OpenGL only for capture rows" + harness page section | Implementer + manager |
| G15 (P3) | Evidence schema, QQ-007 sub-outcomes, determinism caveats (clock applet, fonts) | Section 7 schema; weights proposal in Section 9 | Manager |

## 5. Ordered vertical slices

Each slice is independently reviewable, adds one CTest row family under `desktop.virtual.*`, and never starts private runtime without the manager's serialized lane.

- **S0 — Sandbox and audit primitives (no runtime).** `desktop_session_sandbox.py` (bwrap argv builder from a typed spec; fallback builder; `--print-command`), `desktop_session_measure.py` (smaps_rollup/stat parsers), host audit function, environment allow-list. Row `desktop.virtual.sandbox-unit` (pure Python; fixtures for `smaps_rollup`, `/proc` layouts, argv golden files). Acceptance: 100 % of builders covered; `--print-command` output contains no host socket paths and no `DISPLAY`/`WAYLAND_DISPLAY`.
- **S1 — Boot + teardown at 1080p.** Row `desktop.virtual.boot.1080p`: stage install; sandbox; Settings1 explicit start; `qindaqt-wm --virtual … --test-scenario single-1080p.json --session "qindaqt-session --profile qindaqt --theme qinda-dusk"`; await `org.qindaqt.Compositor`, `org.freedesktop.Notifications` (host), `org.qindaqt.Settings1`, shell surfaces (via `ShellVisibilitySnapshot`/`Windows`), `org.qindaqt.Audio1` after one call; start `qindaqt-settings`; assert `InputCapabilities` = one development device, `Outputs` = 1920x1080@1; measure PSS/CPU; graceful teardown; host audit. `KWIN_COMPOSE=Q`. TIMEOUT 180, `RUN_SERIAL`.
- **S2 — Confined interaction.** Row `desktop.virtual.interact.1080p`: two painted windows + Settings app; inject pointer move/click on window B → `Windows()` active window changes; `Meta+N` → notification center visible (Soren's `DevelopmentShellSurfaces` if integrated, else `Windows()`/`ShellVisibilitySnapshot`); `Escape` closes; record `deviceId`, event counts, effects. Requires G4 (or limit to LeftMeta/Enter/Down until integrated).
- **S3 — Screenshots at 1080p/WUXGA/1440p.** Rows `desktop.virtual.capture.{1080p,wuxga,1440p}`: `KWIN_COMPOSE=O2`, render node bound, `KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1`; captures at `boot-idle` and `after-interaction`; sidecars; dimension asserts. Plus `desktop.virtual.production-read-only` (2.4).
- **S4 — Variants.** Rows `desktop.virtual.capture.{1080p-125,1080p-150,1440p-125,dual-1080p}` using the catalog's profile/theme per scenario (F9), so light/dusk/dark and four profile families are covered across the 1080p rows; dual-1080p captures `Virtual-0` and `Virtual-1`.
- **S5 — Repeatability, docs, ADR.** `desktop.virtual.boot.race-10` (`--repeat 10`, fresh XDG tree each, TIMEOUT 1800); harness page section replacing the "remain required" sentence at `testing-harness.md:61-65` with what now exists; ADR (G14); `features.json` update is the manager's.
- **S6 — Later.** G10 CI capture seam; G12 private PipeWire; D0-backed heterogeneous/rotated rows; keyboard allow-list; pixel baselines with pinned fonts/clock.

## 6. Failure containment

- Per-phase deadlines inside the inner driver (compositor service ≤15 s, host/shell ≤15 s, Settings1 ≤10 s, Audio1 ≤10 s, app ≤10 s, capture ≤10 s/output, measure 40 s); each timeout produces a typed failure in the evidence and proceeds to teardown.
- Any survivor, missing artifact, non-zero compositor exit on the graceful path, wrong output geometry, wrong device inventory, or PSS above ceiling fails the row; nothing is skipped silently. Exit 77 is reserved for "sandbox mode requested but `bwrap`/userns missing" **only** when the fallback was not configured, mirroring `SKIP_RETURN_CODE 77` semantics at `tests/session/CMakeLists.txt:337,377`.
- Logs are bounded (8 KiB diagnostic cap pattern, `shellsurfaceprobe.cpp:33`) and never mixed into the stdout JSON marker.
- Private runtime remains single-lane (`OPERATING_MODEL.md` "Parallelism and quality"); rows are `RUN_SERIAL TRUE` and the outer driver refuses to start if another `desktop-session-runs/*/lock` is held.

## 7. Evidence JSON (one object per row on stdout, mirrored to `artifacts/evidence.json`)

```json
{"schemaVersion":1,"runId":"…","scenario":"single-1080p","logicalSize":[1920,1080],"scale":1.0,"outputCount":1,
 "containment":{"mode":"bwrap-pid-net","hostRuntimeDirMasked":true,"devUinputAbsent":true,"devInputAbsent":true,
   "envScrubbed":["DISPLAY","WAYLAND_DISPLAY","WAYLAND_SOCKET","XAUTHORITY","DBUS_SESSION_BUS_ADDRESS","QINDAQT_DOTOOL"],
   "tools":{"dbusRunSession":"/usr/bin/dbus-run-session","python":"/usr/bin/python3","bwrap":"/usr/bin/bwrap"}},
 "processes":{"compositor":0,"session":0,"notificationHost":0,"shell":0,"settingsService":0,"audioService":0,"xwayland":0,"settingsApp":0,"dbusDaemon":0},
 "compositor":{"kwinAbi":"6.6.5","controlMode":"development-test","compositing":"OpenGL|QPainter","outputs":[],"inputDevices":["QindaQt Development Input"]},
 "interaction":{"deviceId":"…","batches":0,"effects":[{"action":"click-window-B","observed":"activeWindow=B"}]},
 "screenshots":[{"output":"Virtual-0","phase":"boot-idle","file":"captures/boot-idle-Virtual-0.png","width":1920,"height":1080,"scale":1.0,"format":5,"sha256":"…"}],
 "memory":{"samples":3,"aggregatePssKiB":0,"ceilingKiB":1048576,"perProcess":[{"pid":0,"comm":"kwin_wayland","pssKiB":0}],"idleCpuPercent":0.0,"cpuWindowSeconds":30},
 "teardown":{"compositorExitCode":0,"sandboxExitCode":0,"survivorsAfterExit":0,"durationMs":0},
 "passed":true}
```

## 8. Executable acceptance commands (for the implementer/manager; not run by me)

```sh
# build (serial lane)
cmake --preset dev && cmake --build --preset dev --parallel 1
# S0 — no runtime
ctest --test-dir build/dev -R '^desktop\.virtual\.sandbox-unit$' --output-on-failure
python3 tests/session/desktop_session_sandbox.py --print-command --run-root build/dev/tests/session/desktop-session-runs/dry \
  | grep -E 'DISPLAY|/run/user|\.X11-unix|uinput' ; test $? -eq 1   # must print nothing host-reachable
# S1..S5 — serialized private runtime lane only
ctest --test-dir build/dev -R '^desktop\.virtual\.boot\.1080p$' --output-on-failure
ctest --test-dir build/dev -R '^desktop\.virtual\.interact\.1080p$' --output-on-failure
ctest --test-dir build/dev -R '^desktop\.virtual\.capture\.(1080p|wuxga|1440p)$' --output-on-failure
ctest --test-dir build/dev -R '^desktop\.virtual\.(capture\.(1080p-125|1080p-150|1440p-125|dual-1080p)|production-read-only)$' --output-on-failure
ctest --test-dir build/dev -R '^desktop\.virtual\.boot\.race-10$' --output-on-failure
# host audit after any row (must print nothing)
grep -lz "QINDAQT_SESSION_RUN_ID=$RUN_ID" /proc/[0-9]*/environ 2>/dev/null
# regressions that must stay green on the same tree
ctest --test-dir build/dev -R '^shell\.production-surface\.(1080p|wuxga|1440p)$' --output-on-failure
ctest --test-dir build/dev -R '^(session\.(nested-wayland-xwayland|virtual-output\.)|compositor\.(kwin-plugin-nested|production-control-read-only))' --output-on-failure
python3 tools/docs_validation.py --root . && python3 -m tools.source_shape.cli && git diff --check
```

## 9. P0–P3 list

- **P0-1** Screenshot path under QPainter does not exist in KWin 6.6.5 while all nested rows force `KWIN_COMPOSE=Q` (U2, F6). Decision: capture rows enforce `O2` on a bound render node; G10 later. Blocks ADR-0015 "captured screenshots" until decided.
- **P0-2** No whole-session entry point composes compositor + `qindaqt-session` + Settings1 + Audio1 + apps on one private bus with one evidence object (ADR-0015 `:42-43`; F6 shows probes standing in for the session). Slice S1.
- **P0-3** Teardown and host containment are not kernel-enforced; `subprocess.run` timeouts kill only `dbus-run-session`; `/dev/uinput` is reachable by this user (F6, H3). Slice S0 sandbox + fallback.
- **P1-1** Input vocabulary is four keys (F5); adopt Soren's extension (G4).
- **P1-2** `--no-lockscreen`/`--no-global-shortcuts` remove KSldApp/KGlobalAccelD from the nested session; whole-desktop rows must omit them and write the private locker policy (G5).
- **P1-3** `qindaqt-session` lacks a SIGTERM→quit bridge; graceful, exit-0 shutdown needs G6.
- **P1-4** Tool resolution is environment-dependent (F10); pin `/usr/bin` tools and record them (G7).
- **P1-5** `hide-caller-windows` default hides the probe's windows; cursor excluded by default (U1; G8).
- **P1-6** Six catalog scenarios are unrepresentable by the CLI (F9); choose representable rows, route the rest to D0 (G9).
- **P2-1** CI-deterministic QPainter capture seam (G10). **P2-2** Launcher/dev-tool env hygiene (G11). **P2-3** Private PipeWire for Audio1 (G12). **P2-4** Multi-output shell publication boundary (G13). **P2-5** `tests/controls/measure_pss.py` exists only in the manager's staged Controls integration — evaluate for smaps parsing reuse after it lands. **P2-6** ADR for the sandbox dependency and compositing policy (G14).
- **P3-1** Evidence schema adoption (Section 7). **P3-2** After S1 is stable, add `--unshare-ipc` and a `$HOME` tmpfs with explicit worktree/build binds. **P3-3** Race/CPU rows on the reference machine only (F11). **P3-4** Register rows through `DesktopSessionTests.cmake` (source-shape). **P3-5** Determinism caveats for later pixel baselines: clock applet time, fonts (`fc-match` recorded), wallpaper. **P3-6** Proposed QQ-007 sub-outcomes for the manager's rubric: `.01` private nested boot/teardown 20, `.02` confined interaction 15, `.03` screenshot matrix (resolution/DPI/theme/multi-output) 20, `.04` idle PSS/CPU measurement 10, `.05` physical hardware 20, `.06` packaging/migration/recovery/upgrade 15 (= 100).

## 10. Open items I could not verify (labelled inference; first slice must confirm)

- bwrap sets `PR_SET_NO_NEW_PRIVS` in unprivileged mode (upstream bubblewrap behaviour). Consequence either way: `kwin_wayland`'s `cap_sys_nice=ep` is either honoured inside the user namespace or silently dropped; neither blocks the QPainter/EGL virtual backend (CI already runs KWin without `SYS_NICE`, F11).
- KWin exits 0 on `SIGTERM` (KSignalHandler → quit) so the graceful path yields `compositorExitCode 0`.
- EGL/GBM on `amdgpu`'s render node works inside the sandbox with only `renderD128` bound (libdrm enumeration reads sysfs, which `--dev-bind / /` keeps visible).
- Virtual connector names for CLI-created outputs are `Virtual-<index>` (the existing rows' `kwinoutputconfig.json` uses `Virtual-{index}` and passes, F6).
- `ksycoca` builds on demand for the desktop-file authorization route (deferred; env route recommended).

**Requested action:** manager routes G1–G3 (S0/S1) to a harness implementer with the serialized private-runtime lane; Soren's G4/G6-adjacent diffs are integrated before S2; Rhea's D0 seam is not on the S1–S5 critical path. I remain available to exact-review the S0/S1 candidate commit.
