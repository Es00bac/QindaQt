# Dorian Vale — exact Notification Live candidate PASS

- **Timestamp:** 2026-08-28T10:54:54Z
- **Reviewer:** Dorian Vale, independent KWin API and nested-session evidence auditor
- **Exact commit reviewed:** `557260a50faaf083733afe5972ad6541ef398108`
- **Exact tree:** `8f9f131461157b33bb88e0b4a46811e2308c9329`
- **Exact parent:** `c4982697858c083828bd406f1aa56c4e942bcc10`
- **Verdict:** **PASS**
- **Findings:** P0 0, P1 0, P2 0, P3 0

## Immutable identity and scope

I reviewed the commit, not Soren's prose. In a clean detached review worktree I
recomputed the commit, tree, and parent above; 74 changed paths; 5,825 additions
and 224 deletions; and the sorted path-manifest SHA-256
`3be3d516f941c62d0d8f227258d0669fe71e336d787af9e7da3435755a98e731`.
All match the handoff. `git diff --check` is clean and the detached candidate
has no product changes. Build output is ignored and outside the immutable tree.

## Boundary and adversarial review

- **KWin exported ABI/plugin load:** the plugin tracks exported
  `LayerSurfaceV1Interface` objects and resolves public `Window` objects through
  `WaylandServer::findWindow`; it never calls the non-exported
  `LayerShellV1Window` boundary (`src/compositor/kwin/kwincontrolendpoint.cpp:110-126`,
  `:213-255`). Fresh staged Debug `ldd -r` has no not-found/undefined symbol,
  `nm -D -C` contains the expected exported API references and no
  `LayerShellV1Window`. Development input goes through KWin's real input-device
  registration and releases all held keys/buttons before removal
  (`src/compositor/kwin/kwindevelopmentinputinjector.cpp:140-210`).
- **Geometry, focus, properties, and presentation:** the window controller
  creates QML with required initial properties, configures production
  layer-shell surfaces, recomputes geometry on state changes, and maps popup or
  center only from production presentation state
  (`src/shell/runtime/notificationwindowcontroller.cpp:246-303`, `:306-353`).
  Read-only evidence records actual window geometry, output, active focus and
  both focus directions with a bounded cycle (`:69-130`). Deferred initial
  focus is seeded only after the center becomes active
  (`src/shell/qml/NotificationCenter.qml:19-28`).
- **Settings route containment:** only explicit private development mode plus
  an absolute executable selects the non-detached child; teardown terminates
  and reaps that child. Ordinary production retains the normal detached
  `qindaqt-settings --page notifications` route
  (`src/shell/runtime/settingsroutelauncher.cpp:11-74`).
- **Supervisor lifecycle:** one in-memory token starts the resident host and
  shell; stop clears authority and both children (`src/session_supervisor/src/session_process_supervisor.cpp:63-113`).
  A first shell exit can start one replacement with a fresh one-shot descriptor,
  the same in-memory token and compositor PID; host loss, replacement launch
  failure, or a subsequent exit tears down the remaining child (`:145-237`).
  This implements ADR-0019's one-restart/host-continuity/no-replay boundary
  (`docs/wiki/adr/0019-restart-the-production-shell-once.md:28-80`).
- **Evidence authentication:** the shell endpoint requires the exact
  development marker, a connected bus, the compositor service's exact
  supervisor-provisioned PID, and live development capabilities before it can
  register (`src/shell/runtime/shelldevelopmentevidence.cpp:107-162`). ADR-0020
  correctly constrains that endpoint to read-only snapshots, external private
  bus/XDG containment, exact PID authentication, non-queueing predecessor
  release and unique-owner locker calls
  (`docs/wiki/adr/0020-authenticate-private-live-evidence.md:23-65`, `:73-105`).
- **Private bus/socket/input/lock and teardown:** every repetition receives a
  fresh temporary root and `dbus-run-session`
  (`tests/session/notification_live_outer.py:73-113`). The inner driver refuses
  inherited display or session-bus state and confines/read-backs the exact
  private locker policy (`tests/session/test_notification_live_nested.py:146-172`).
  It starts only staged artifacts and authenticates the exact compositor, host,
  settings and shell owners before the six-phase workflow (`:247-348`). The
  outer runner validates a new-session process group, applies bounded TERM/KILL,
  and proves no grandchildren remain even after successful leader exit
  (`tests/session/notification_live_process.py:31-147`).
- **Package, tests, docs, and modularity:** the CMake registration requires all
  staged production targets, names the exact installed artifacts, registers
  1080p/WUXGA/1440p/125%/150% plus ten fresh race repetitions, serializes them,
  and labels them installed/security/Wayland integration
  (`tests/session/NotificationLiveTests.cmake:12-122`). ADRs 0019/0020,
  compositor/session, notification service/presentation, control reference and
  testing-harness documentation agree with the code. Source-shape validation
  reports 800 files with no violations; the largest product file is 493
  nonblank lines and the largest changed production source is 478, so no new
  monolithic boundary exception is hidden here.

## Independently replayed evidence

- Fresh exact Debug build completed, followed by the full install graph.
- Focused static/regression CTest: **11/11 PASS** — Compositor1 contract,
  development-input protocol/injector, runtime options, live-driver unit,
  Python syntax, session supervisor, quieting bridge, and the notification
  surface/focus/quieting-control offscreen rows.
- Documentation/source gates: **44 docs PASS**, **800 source files PASS**, and
  strict MkDocs PASS.
- Fresh private installed matrix: **5/5 PASS in 49.19 s**:
  1080p 10.78 s, WUXGA 9.62 s, 1440p 9.63 s, 125% 9.53 s, 150% 9.62 s.
  Every result contains all six passing phases, real forward/reverse focus,
  shortcut disable/remap/restore, DND suppression and critical bypass,
  Settings1 rejection/uncertain/outage/restart, private lock privacy/no replay,
  one fresh authenticated replacement shell, and a continuous host PID.
- Fresh private race: **10/10 complete repetitions PASS in 93.49 s**. The
  preserved JSON reports `repetitions: 10`; every run is passed, uses a fresh
  replacement shell, keeps its host PID continuous, and ends with fresh shell
  authentication.
- I also inspected Soren's exact post-extraction Release 1080p CTest log:
  **PASS in 9.86 s** at this immutable commit. My fresh Debug 1080p replay above
  independently removes reliance on that preserved result.
- Final teardown: **0** `/tmp/qindaqt-notification-live-*` roots, **0** process
  command lines referencing the detached review/staging roots, and a clean
  staged-plugin ABI scan. No host display, cursor, input seat, session bus,
  config, locker, password, or hardware was accessed.

## Qualification decision

**QQ-004.05 qualifies at `QUALIFIED` maturity** on this exact commit. The
accepted outcome is the production Notification Live vertical slice: installed
shell/host/Settings1/supervisor/KWin composition, real virtual-Wayland
interaction, keyboard/focus/shortcut behavior, DND and critical behavior,
settings failures and recovery, private lock privacy, one-shell restart with
resident-host continuity, authenticated replacement, no replay, and bounded
teardown across the five required rows plus ten fresh race repetitions.

This verdict is deliberately limited to QQ-004.05. It does not claim visual
screenshot baselines, full accessibility/screen-reader coverage, physical GPU
or input, host-session locking, multi-seat/session switching, alternative
lockers, suspend/resume, or mixed physical multi-output behavior. Those are
separate whole-desktop or later qualification outcomes, not defects in this
candidate's declared slice.

**Requested next action:** the manager may integrate this exact commit, rerun
the affected gates on the combined tree, and record QQ-004.05 as `QUALIFIED`.
