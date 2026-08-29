# Omar Finch six-axis containment/teardown audit — bounded pass, no blocker

- **To:** Soren Pike, lead/keeper, Notification Live qualification
- **From:** Omar Finch, containment QA assistant
- **Timestamp:** 2026-08-27T23:15:41-06:00
- **Audited state:** `worker/notification-live` at base/HEAD
  `c4982697858c083828bd406f1aa56c4e942bcc10`, uncommitted 70-path candidate
  (38 tracked + 32 untracked). Fresh re-derivation, not a reuse of my
  69-path audit `1787873998`. Read-only; no product/Git edit, compile,
  install, nested launch, input, or host-session contact.

## Verdict

No static blocker on any containment or teardown axis. I recommend a bounded
pass for the containment/teardown surface, with the runtime-only residues in
section 8 reserved for your private nested execution, as you already plan.

## 1. Private runtime/bus/socket identity — PASS

- Per-repetition fresh `tempfile.TemporaryDirectory` + `isolated_environment`
  strips `DBUS_SESSION_BUS_ADDRESS`, `DISPLAY`, `WAYLAND_DISPLAY`, dev
  markers, and `QINDAQT_DOTOOL`, and creates a 0700 `runtime`
  (`tests/session/nested_session_scenario.py:146-181`); the repetition runs
  `dbus-run-session` in one new session/group
  (`tests/session/notification_live_outer.py:77-100`).
- Triple refusal: inner driver (`tests/session/test_notification_live_nested.py:161-168`),
  C++ probe (`tests/session/notificationliveruntime.cpp:111-117`), and the
  launcher re-guard (`src/session/sessionenvironment.cpp:23-31` unsets both
  markers, re-arms only with `--test-scenario`), enforced again inside KWin by
  `src/compositor/kwin/mutationcontrol.cpp:8-13`.
- Staging is the only rmtree and is guard-confined to a child of the build
  tree (`tests/session/notification_live_stage.py:26-30`); artifact resolution
  cannot escape the prefix (`:12-19`; unit-tested
  `tests/session/test_notification_live_unit.py:73-88`).
- `PATH` prepends only the staged bindir; `dbus-runner`/`busctl` arrive as
  absolute CMake-found paths
  (`tests/session/NotificationLiveTests.cmake:85-86`).

## 2. Synthetic input target — PASS

- Every key enters through private-bus `InjectTestInput`
  (`tests/session/hybridtestinputdriver.cpp:321-347`) into the nested KWin
  in-process `qindaqt-development-input` device. Production rejects the method
  before parsing (`src/compositor/kwin/developmentinputprotocol.cpp:220-226`);
  the injector object is never constructed without the marker+scenario pair
  (`src/compositor/kwin/qindaqtkwinplugin.cpp:41-47`).
- The `DotoolProcess` host-uinput route (`hybridtestinputdriver.cpp:52-219`)
  is referenced by no notificationlive* file, and `QINDAQT_DOTOOL` is stripped
  from the harness environment. The probe requires the exact device id before
  use (`notificationliveruntime.cpp:137-145`) and pins device identity across
  each gesture (`hybridtestinputdriver.cpp:339-344`).
- Teardown clears held keys/buttons before device removal
  (`src/compositor/kwin/kwindevelopmentinputinjector.cpp:140-171,201-214`).

## 3. DND replacement — PASS (both readings)

- DND transaction: keyboard toggle through the production controller
  (`notificationliveworkflow.cpp:134-152`); visible `Saving…` with the private
  Settings1 SIGSTOPped (`tests/session/notificationlivesettingsphases.cpp:124-181`);
  confirmed rejection via a 0o500 private config dir
  (`test_notification_live_nested.py:185-201`); uncertain state after
  re-authenticated PID + session-checked SIGKILL
  (`notificationlivesettingsphases.cpp:142-190` with
  `notificationliveruntime.cpp:69-98`); outage last-confirmed truth
  (`:220-262`); fresh-owner recovery with DND still enabled and no replay
  (`:313-377`); counters prove QML-visible status
  (`src/shell/runtime/shelldevelopmentevidence.cpp:398-427`).
- Evidence-name replacement: restart limit 1, predecessor PID passed only in
  development mode (`src/session_supervisor/src/session_process_supervisor.cpp:143-167`);
  replacement waits ≤1 s for the exact predecessor release, then registers
  `DontQueueService|DontAllowReplacement` — never queues, never evicts
  (`shelldevelopmentevidence.cpp:165-251`, guard at `:236-242`); the driver
  kills the first shell only after re-authenticating owner+session
  (`test_notification_live_nested.py:342-344`,
  `tests/session/notification_live_process.py:190-207`) and requires the
  replacement PID to differ (`:345-347`).

## 4. Lock authentication — PASS

- Both locker names and the compositor must share one owner equal to the exact
  nested KWin PID before any locker call
  (`tests/session/notificationlivelock.cpp:93-112`); `Lock()`/`GetActive()`
  then address that authenticated unique owner, not the well-known name
  (`:121-132`).
- `RequirePassword=false` is written only beneath the private config root with
  resolve-containment and byte readback
  (`test_notification_live_nested.py:142-158`; unit-tested
  `test_notification_live_unit.py:104-115`). No host config, PAM, or password
  path participates; unlock is development-device user activity only
  (`notificationlivelock.cpp:164-171`).
- Locked denial is proven by a center-open-count invariant plus cleared
  presentation (`:147-158`), critical+transient submission denied
  (`:159-163`, `:18-37`), and double-inactive unlock baselining with no
  replay (`:172-184`).

## 5. Crash/timeout cleanup — PASS

- Exact-group teardown refuses self/non-leader targets
  (`notification_live_process.py:30-43`), then TERM→3 s→KILL→2 s and hard
  failure if any group member survives (`:74-107`); it runs unconditionally
  after success too (`:145`), and the timeout path tears down before failing
  (`:130-139`).
- Inner `finally` terminates the execvp'd KWin and Settings processes
  (`test_notification_live_nested.py:375-379`); a stuck probe is killed by
  `subprocess.run(timeout=45)` (`:95-97`); supervisor stop is idempotent with
  a reentrancy guard (`session_process_supervisor.cpp:96-113`); host loss
  ends the session without a shell restart, and restart failure stops the
  sibling (`:169-194`, `:83-90`); PDEATHSIG re-verified at
  `src/session_supervisor/src/tokenized_process_launcher.cpp:54-61`.
- Budgets compose: probe 45 s inside the 180 s inner budget; CTest 240 s per
  row and 2400 s for race-10x with a unit-parsed ≥300 s margin guard
  (`NotificationLiveTests.cmake:68-75`,
  `test_notification_live_unit.py:42-57`). C1 from my prior audit is closed.
- Bounded caveats, unchanged fail-safe direction: (a) an external hard kill of
  the outer driver orphans the private session until its own members exit —
  containment still holds, only cleanup is lost; (b) the prior C2-C5 items
  (negative-window, focus-race, 45 s probe budget, duplicate/absent
  diagnostics) remain accepted timing/diagnostic caveats.

## 6. Proof the host session cannot be reached — PASS (static)

- Defense in depth: environment stripping → inner refusal → probe refusal →
  every privileged D-Bus action bound to a bus PID previously matched to an
  externally observed nested PID (compositor/host/settings/kglobalaccel/
  locker/shell: `tests/session/notificationliveworkflow.cpp:38-76`).
- All signal targets must live in the driver's own session
  (`notificationliveruntime.cpp:69-98`; Python mirror
  `notification_live_process.py:46-63`), re-checked immediately before
  killing (`notification_live_process.py:190-207`).
- KGlobalAccel mutations go to the private bus only, with
  globalAccelPid==compositorPid lineage asserted first
  (`notificationliveworkflow.cpp:60-64`) and restore armed before mutation
  (`:163-180`). Shell evidence is production-disabled and four-way gated
  (`shelldevelopmentevidence.cpp:107-163`); `DevelopmentShellSurfaces`
  rejects `control-disabled` before inspecting KWin in production
  (`src/compositor/kwin/kwincontrolendpoint.cpp:193-202`); every snapshot is
  re-bound to the authenticated shell PID
  (`tests/session/notificationliveevidenceclient.cpp:84-104`).

## 7. Direct gate I ran

`PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=tests/session python3
tests/session/test_notification_live_unit.py` → exit 0, 10/10, no tree
mutation (re-verified `git status` unchanged: 70 candidate paths + tool-local
`.omc/`, which is not candidate content — keep it out of any future `git add`).

## 8. Runtime-only residues reserved for your private run

1. execvp'd KWin honoring the private env end to end (marker re-arm, sockets
   only in the private runtime dir).
2. KScreenLocker 6.6.5 immediate `Lock()` and password-disabled unlock via
   device activity.
3. Adequacy of the ≤1 s predecessor name-release window on the one shell
   replacement.
4. race-10x wall clock inside the 2400 s CTest budget.
5. Exact live scale reporting on the 125%/150% rows.

## Next action

No containment repair requested. Proceed with your recorded compiler-owned
focused gate, then the private nested matrix; I remain available for a
re-audit of any commit or of the four-path integration collision set.
