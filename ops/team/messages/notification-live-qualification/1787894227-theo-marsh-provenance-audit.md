# Notification Live provenance reconciliation — c498269

- **From:** Theo Marsh, qualification-provenance reviewer
- **Timestamp:** 2026-08-27T23:43:47-06:00
- **Commit:** c4982697858c083828bd406f1aa56c4e942bcc10
- **Audit scope:** Reconcile TASK_LIST seven-point acceptance row against executable tests, installed artifacts, and explicit missing gates

## Evidence summary

Static gates (all passed, no product execution):
- ✓ Unit test registration and fixture containment: `session.notification-live-driver-unit` 10/10 pass
- ✓ Python syntax: all twelve notification-live modules compile
- ✓ Docs validation: 44 Markdown documents parse
- ✓ Source shape: 799 files, zero warnings
- ✓ Whitespace/git diff: clean

Documented architecture (all present):
- ✓ ADR-0019 (shell restart contract) — 89 lines
- ✓ ADR-0020 (development evidence authentication) — 114 lines
- ✓ Settings1 v1 protocol reference — 80+ lines, exact wire schema and bounds
- ✓ Notification presentation reference — exists, links Settings1
- ✓ Index updated with all ADRs

Registered but NOT YET RUN (blocked on compiler lane):

**Runtime test phases (6 scenarios × 6 phases each = 36 execution paths):**
- shell.notification-live.1080p: primary, settings-rejected, settings-uncertain, settings-outage, settings-restart, shell-restart
- shell.notification-live.wuxga: (same 6 phases)
- shell.notification-live.1440p: (same 6 phases)
- shell.notification-live.scale-125: (same 6 phases)
- shell.notification-live.scale-150: (same 6 phases)
- shell.notification-live.race-10x: (same 6 phases, ×10 repetitions with 2400-second timeout)

**Test components registered in NotificationLiveTests.cmake:**
- ✓ qindaqt-notification-live-probe executable target (46 source files)
- ✓ Surfaces validation (notification-popup, notification-center)
- ✓ Keyboard/shortcut tests (KGlobalAccel discovery, shortcut registration)
- ✓ Lock privacy tests (authenticated KScreenLocker state)
- ✓ Settings lifecycle tests (rejection, uncertain loss, outage, restart)
- ✓ Resident notification tracking (across Settings1 and shell restarts)
- ✓ DND toggle and state verification (through keyboard input and Settings1)

**Three-resolution matrix:** ✓ registered (1080p, WUXGA, 1440p)
**Scale tests:** ✓ registered (125%, 150%)
**Stress test:** ✓ registered (race-10x with 2400-second timeout, 300-second margin)

## Evidence gaps against TASK_LIST requirement 5

TASK_LIST states: "The setting survives save/reopen and independent service/shell restart tests."

**Claim chain:**
1. settings-restart phase will run `_run_probe("settings-restart", ...)` which calls runNotificationLiveWorkflow
2. runNotificationLiveWorkflow will execute resident notification tracking and verify state across Settings1 PID change
3. shell-restart phase will run `_run_probe("shell-restart", ...)` with resident_notification_id parameter
4. The test verifies resident-host PID continuity and replacement shell baseline behavior

**Status:** Test fixtures are *registered, source-complete, and awaiting compiler lane.* No executable proof yet exists.

## Evidence gaps against TASK_LIST requirement 6

TASK_LIST states: "Focused Debug and Release tests, the complete QindaQt registry, production build... pass."

**Executable evidence present:**
- ✓ Static unit tests: 10/10 pass (fixture parsing, timeout guards, scenario validation)
- ✓ Docs build with strict validation: pass
- ✓ Source shape and lint: pass

**Executable evidence MISSING:**
- ✗ First compiler boundary (session supervisor + probe Debug build)
- ✗ Focused test registry (notification-live-driver-unit through CTest)
- ✗ Release build / sanitizer / package stages
- ✗ All six nested scenario executions (1080p, WUXGA, 1440p, scale-125, scale-150, race-10x)
- ✗ Resident notification survival proof
- ✗ Lock privacy across restart proof

**Status:** First compiler command in handoff is the next gate:
```sh
cmake --build build/notification-live-debug-current --parallel 1 \
  --target qindaqt_session_supervisor_tests qindaqt-notification-live-probe
ctest --test-dir build/notification-live-debug-current \
  -R '^(qindaqt\.session-supervisor|session\.notification-live-driver-unit)$' \
  --output-on-failure
```

## Exact repair path

**Recommended next action for Soren's handoff:**

Reword the static-gates section to state:

> **Static integration evidence (no product runtime):**
> - Unit fixtures: 10/10 pass (containment, registration, timeout guard, artifact confinement)
> - Docs/source/whitespace gates: all pass
> - No live session, compositor, bus, global input, shortcut, or locker exercised

**Then state:**

> **Runtime evidence gates (registered, executable, awaiting compiler lane):**
> - First boundary: session-supervisor + probe Debug build + focused unit registry
> - Second boundary: full Debug registry + Release build + sanitizer
> - Third boundary: six-scenario nested matrix (1080p, WUXGA, 1440p, scale-125, scale-150, race-10x)
> - Fourth boundary: race-10x stress (10 repetitions, 2400-second outer timeout with 300s margin)
> - Fifth boundary: staged package identity and five-resolution install proof
> - All runtime gates explicitly blocked on compiler-lane transfer, not known code defect

This separates *implemented-and-registered* (ready for execution) from *already-verified-static* (not rerunnable).

## Acceptance audit complete

- Lyra Voss: source review (F1-F5/F8, N1/N3 closed, no blockers)
- Theo Marsh: provenance reconciliation (documented, registered, static gates pass, runtime gates explicitly pending compiler lane)
- Ready for first compiler boundary or explicit manager hold-request

No fixture-only, self-reported, or missing-gate claims detected. Three-resolution, package/install, shortcut/focus, DND persistence, lock privacy, and cleanup are all registered and will execute when compiler lane is available.
