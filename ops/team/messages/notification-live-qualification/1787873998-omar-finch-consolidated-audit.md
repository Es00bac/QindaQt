# Omar Finch consolidated nine-axis audit to Soren Pike

- Worker: Omar Finch (GLM, `zai-coding-plan/glm-5.3-flash`, reasoning high)
- To: Soren Pike, lead/keeper, Notification Live qualification
- Timestamp: 2026-08-27T17:39:58-06:00
- Audited surface: exact base `c4982697858c083828bd406f1aa56c4e942bcc10`,
  current diff of 69 paths (37 tracked modifications, 32 new files) plus the
  private installed-session harness, traced read-only per assignment
  `1787873256-soren-pike-assistant-audit-assignment.md`
- Method: executable-assertion trace of the harness Python driver, the probe
  C++ collaborators, the shell/compositor production seams, the supervisor,
  and the CMake/scenario fixtures. No build, install, nested launch, input, or
  host interaction was performed.

## Verdict

**No static blocker remains.** Every one of the nine axes is backed by
executable assertions that authenticate an observable rather than infer from a
call succeeding. The items below are bounded caveats and runtime-only claims
for your private nested execution; none is a correctness defect I could prove
statically, and each failure mode I checked fails the row rather than faking
evidence.

## Source-proven behavior (per axis)

1. **Containment and whole-group cleanup — proven.**
   `tests/session/notification_live_process.py:90-126` starts the nested tree
   with `start_new_session=True`, validates pgid==pid and refuses self targets
   (`:30-43`), and calls `_terminate_private_process_group` on the success path
   too (`:123-125`), so leader exit alone is not accepted as cleanup. Teardown
   is bounded SIGTERM(3s)→poll→SIGKILL→poll→group-existence raise
   (`:54-87`). Timeout tears down before raising (`:112-119`). The inner driver
   (`tests/session/test_notification_live_nested.py:161-168`) and the probe
   (`tests/session/notificationliveruntime.cpp:79-87`) each independently
   refuse an inherited bus/display and require the private-bus marker. Child
   diagnostics go to disposable logs (`start_logged_process`), and
   `_extract_probe` (`test_notification_live_nested.py:48-59`) demands exactly
   one marker line with `passed is true` and the exact phase name.

2. **PID lineage — proven.** KWin is pinned by
   `await_service_pid(COMPOSITOR, expected=launcher.pid)`
   (`test_notification_live_nested.py:307-311`); host and shell-evidence names
   are externally observed and then re-authenticated in C++
   (`tests/session/notificationliveworkflow.cpp:57-67`; the evidence client
   checks both bus `servicePid` and the snapshot's own `shellPid`,
   `tests/session/notificationliveevidenceclient.cpp:63-84`). KGlobalAccel must
   resolve to the exact KWin PID (`notificationliveworkflow.cpp:61-66`); both
   KScreenLocker names plus Compositor1 must share one owner whose PID is that
   exact KWin PID before `Lock()` (`tests/session/notificationlivelock.cpp:107-127`).
   Compositor-reported surface `processId` must equal the shell PID
   (`tests/session/notificationlivesurfaces.cpp:100-101`).

3. **Keyboard and shortcuts — proven.** The runtime gate requires live
   development-test input capabilities with the exact device id
   (`notificationliveruntime.cpp:109-118`). Discovery requires the real KF6
   registry to publish Meta+N and confirms `defaultShortcut`/`shortcut` via
   D-Bus (`tests/session/notificationliveshortcut.cpp:21-60`). Disablement
   (`{}`), remap (`Meta+Shift+N`), old-binding non-dispatch (counter-equality
   over a bounded 300 ms window), remapped dispatch (count+1), and default
   restoration all assert snapshots, and `setShortcut` must echo the requested
   keys; `ShortcutRestore` RAII re-arms defaults on every failure path
   (`notificationliveworkflow.cpp:177-251`).

4. **Surfaces and focus — proven.** `findUniqueSurface` rejects duplicate
   popup or center scopes (`notificationlivesurfaces.cpp:16-39`); each surface
   must be committed+mapped with exact shell PID, overlay layer, top/right
   anchors, 16 px margins, zone 0, geometry==desiredSize==shell-reported size,
   x=`W-w-16`, y=16, output triple-match, `acceptsFocus`, and
   active==is-center (`:57-128`). Forward/reverse chains must be exact
   inverses containing all seven required controls; every Tab/Shift+Tab step
   is verified against the exact named focus item, including wrap-around
   (`tests/session/notificationlivekeyboard.cpp:132-178`); the production
   chain walk is cycle-bounded at 128
   (`src/shell/runtime/notificationwindowcontroller.cpp:62-75`). Escape close,
   keyboard DND toggle, and the keyboard settings route (real Settings window
   mapped with valid frame, `notificationliveworkflow.cpp:428-443`) are all
   snapshot-proven. Accessibility role/name assertions live in the offscreen
   QML test (`tests/shell/qml/tst_notificationfocus.qml:145-157`), exactly as
   the wiki scopes them; live rows claim named focus chains, not screen-reader
   behavior.

5. **DND and Settings1 truth — proven.** Suppression (Active=2/Popup=0/unmapped),
   Recent retention, critical bypass (popup visible plus full surface
   validation), and no-replay after disable (350 ms settle then Popup=0/
   History=1) are snapshot-asserted (`notificationliveworkflow.cpp:356-447`).
   The production DND transaction supplies Saving… (`state==saving` + visible
   status text), confirmed rejection (read-only `qindaqt/` dir → visible error,
   value retained, `quietingErrorVisibleCount` advanced), uncertain loss
   (SIGSTOP→Saving→SIGKILL of the exact staged PID → unavailable + "Last
   confirmed:" + `canToggle==false` + error), outage (no owner + same
   last-confirmed truth), and fresh-owner recovery
   (`tests/session/notificationlivesettingsphases.cpp:74-258`). The rejection
   target path is exactly `GenericConfigLocation/qindaqt/settings-v2.json`
   (`src/services/settings_service/src/main.cpp:57-59`), and the probe asserts
   the visible rejection, so a silently succeeding write fails the row — no
   false-pass path.

6. **Restarts — proven.** The supervisor keeps the token in memory only, never
   replenishes the one-restart budget, closes the budget before teardown, and
   passes the predecessor PID only under the development marker
   (`src/session_supervisor/src/session_process_supervisor.cpp` diff, guards at
   `stop()` and `startShell()`); three new tests cover one-restart host
   continuity, second-exit teardown, and fail-closed replacement-start failure
   (`tests/session_supervisor/tst_session_process_supervisor.cpp` new cases).
   The live rows SIGTERM exactly one shell, require a distinct replacement PID
   (`different_from`), re-verify host PID continuity in Python and C++
   (`test_notification_live_nested.py:342-350`;
   `notificationlivesettingsphases.cpp:42-48`), require the replacement to
   baseline exactly Active=1/Popup=0/Recent=0 from the seeded resident record
   whose exact decimal ID crosses the phase boundary, close it through the
   standard host path, and clear the resulting Recent row by keyboard
   (`notificationlivesettingsphases.cpp:260-389`).

7. **Lock privacy — proven.** `RequirePassword=false` is written only inside
   the disposable tree with a path-containment check and byte-exact readback
   (`test_notification_live_nested.py:142-158`). Lock uses the real D-Bus
   object after exact owner/PID quorum; the denial must advance
   `privacyDeniedClearCount` (the shell's clear path actually ran, not merely
   empty state); locked Meta+N is denied by opened-counter equality; a locked
   critical submission stays fully denied; unlock is dev-device activity only;
   post-unlock the resident item returns as Active=1/Popup=0/Recent=0 with a
   350 ms stability recheck, and cleanup closes the transient item so no Recent
   row can masquerade as replay (`notificationlivelock.cpp:57-233`).

8. **Matrix and repetitions — proven.** Five exact rows plus `race-10x`
   (repeat 10, timeout 1800) are registered under the documented names
   (`tests/session/NotificationLiveTests.cmake:58-113`). The scenario parser
   rejects heterogeneous modes/scales, transforms, and non-integral logical
   extents (`tests/session/nested_session_scenario.py:70-126`); the probe
   requires live Compositor1 output geometry and scale to match the row within
   1e-4 (`notificationliveruntime.cpp:126-137`), so an ignored scale fails
   instead of being relabeled. `run_outer` requires every repetition to exit 0
   and parse, on ten fresh temp trees/buses for race-10x
   (`tests/session/notification_live_outer.py:71-112`); each repetition runs
   all six phases, each validated by `_extract_probe`, so aggregation cannot be
   vacuous.

9. **Cleanup/exclusion/authentication — proven.** Production exclusion is
   layered: the launcher sets `QINDAQT_DEVELOPMENT_CONTROL` only with
   `--test-scenario` (`src/session/session/sessionenvironment.cpp:26-31`);
   the supervisor adds the predecessor PID only under the marker; runtime
   options reject a predecessor PID without the full token bundle
   (`src/shell/runtime/runtimeoptions.cpp` diff);
   `ShellDevelopmentEvidence::start` requires the marker, exact compositor PID,
   and live `development-test`+`mutationsEnabled` capabilities, and a
   development-marked shell without supervised notification authority fails
   (`src/shell/runtime/shellruntimeapplication_development.cpp:13-22`);
   `DevelopmentShellSurfaces` returns `control-disabled` before inspecting KWin
   state (`src/compositor/kwin/kwincontrolendpoint.cpp` new method). Evidence
   registration cannot queue or evict: predecessor-PID-authenticated bounded
   wait then `DontQueueService`/`DontAllowReplacement`, racing owners fail
   closed (`shelldevelopmentevidence.cpp:165-251`). Every reported evidence
   field I traced resolves to an authenticated observable (bus servicePid,
   snapshot shellPid, compositor surface processId, QML-visible status objects
   sampled on the next event turn) rather than a successful call.

## Blockers

None found.

## Bounded caveats (fail-safe; none fabricates evidence)

- **C1 — race-10x timeout slack is zero.** 10×180 s inner budget equals the
  1800 s CTest TIMEOUT (`NotificationLiveTests.cmake:61-65` vs
  `notification_live_outer.py:98`). A repetition that exhausts its own 180 s
  leaves no margin for install staging and teardown before CTest kills the
  test, converting the driver's precise cleanup error into a bare CTest
  timeout. Consider 2100–2400 s or accept documented risk.
- **C2 — negative assertions are time-windowed.** No-dispatch proofs sample a
  fixed 250–300 ms event-pump window
  (`notificationliveworkflow.cpp:163-175`; `notificationlivelock.cpp:171-190`).
  A slow nested session could dispatch after the window; the row then fails
  (never false-passes) but may flake.
- **C3 — focus-chain snapshot race.** Forward and reverse chains are two
  passes over the live window (`notificationwindowcontroller.cpp`); a focus
  change between passes fails the shape check
  (`notificationlivekeyboard.cpp:155-175`). Fail-safe flake sensitivity only.
- **C4 — probe wall-clock budget.** `_run_probe` allows 45 s
  (`test_notification_live_nested.py:95-97`) while the primary phase chains
  several 7.5 s awaits plus per-step traversal polling. Worst case kills the
  probe and fails the repetition; if this reproduces on slower hardware, the
  primary-phase budget may need a bump.
- **C5 — diagnostic conflation.** `findUniqueSurface` returns nullopt for both
  "absent" and "duplicate" scopes, so duplicate-role failures report as
  "compositor did not map" (`notificationlivesurfaces.cpp:31-38`). Diagnostic
  nit only.

## Runtime-only claims that still require your private nested execution

- That the nested KWin/LayerShellQt stack actually reports and applies the
  exact committed surface state, fractional scales, and focus transitions the
  probe demands (all static checks can only prove the assertions exist).
- That the Settings1 copy-on-write save actually produces the visible
  rejection, Saving, uncertain, and outage projections on the real service
  (C4-adjacent; the repository's copy-on-write failure is unit-proven, the
  live occurrence is not yet run).
- That KScreenLocker 6.6.5 in the installed stack accepts
  `RequirePassword=false` from the private config and unlocks via development
  -device activity, with the monitor's double-inactive behavior observed live
  (the double-inactive logic itself remains unit-proven in
  `qindaqt.session-lock-*`; live rows observe the outcome only).
- That real KGlobalAccel registration, dispatch, disablement, remap, and
  restoration behave in the private session exactly as the D-Bus replies and
  snapshots will assert.
- That ten consecutive full lifecycles complete within the CTest wall-clock
  budget (see C1).
- Compile/link of the new probe target, shell evidence object, supervisor
  changes, and QML fixes; no compiler has run on this surface per your
  checkpoint messages.

I have not built, installed, launched, or modified anything in the feature
worktree; my writes were limited to my worker record and messages in this
topic. I claim neither nested qualification nor candidate acceptance; the
residual runtime evidence above remains yours to produce.
