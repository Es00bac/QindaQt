# Theo Marsh: C1 and KF6GlobalAccel scope-repair rereview

- **Reviewer:** Theo Marsh
- **Timestamp:** 2026-08-27T17:49:53-06:00
- **Assignment:** Verify C1 timeout/guard repairs and KF6GlobalAccel scope import
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/notification-live`
- **Base:** `c4982697858c083828bd406f1aa56c4e942bcc10`
- **Prior report:** `1787874245-theo-marsh-qualification-audit.md`

## C1 Timeout Repair: Correct

**File/Line:** `tests/session/NotificationLiveTests.cmake:72-74`

```cmake
# AGENT-GUARD: The outer timeout must exceed ten 180-second inner
# budgets so CTest cannot preempt private process-group teardown.
set(_timeout 2400)
```

- **Previous state:** 1800 seconds (zero margin)
- **Repaired state:** 2400 seconds
- **Margin calculation:** 2400 - (10 repetitions × 180 seconds) = 600 seconds = 10-minute buffer per run cycle
- **Margin adequacy:** 600 seconds accommodates Python startup per cycle, temporary directory creation, D-Bus session lifecycle, staging serialization, and process group cleanup (bounded SIGTERM wait 3s, SIGKILL grace 3s, group-disappearance verification 2s)
- **Status:** ✓ Correct and adequate

## C1 Named Constant: Correct

**File/Line:** `tests/session/notification_live_outer.py:17,99`

```python
REPETITION_TIMEOUT_SECONDS = 180

completed = run_private_process_group(
    [...],
    environment,
    REPETITION_TIMEOUT_SECONDS,
)
```

- **Constant definition:** Line 17
- **Used at:** Line 99 for inner process-group timeout
- **Unit test dependency:** Line 20 of test_notification_live_unit.py imports this exact constant
- **Guard verification:** Unit test extracts actual `_timeout` from CMake and asserts `timeout - (repeat * REPETITION_TIMEOUT_SECONDS) >= 300` (line 52-55)
- **Status:** ✓ Correct and auditable

## KF6GlobalAccel Scope Import: Correct and Safe

**File/Line:** `tests/session/NotificationLiveTests.cmake:5-10`

```cmake
# Imported targets are directory-scoped unless promoted. The shell finds this
# package in src/shell, so tests/session must import it in its own scope before
# using KF6::GlobalAccel or the entire live target would be silently omitted.
if(TARGET qindaqt-shell AND NOT TARGET KF6::GlobalAccel)
    find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)
endif()
```

**Import-order guard:** `tests/session/test_notification_live_unit.py:30-38`

```python
def test_live_registration_imports_directory_scoped_global_accel(self) -> None:
    registration = Path(__file__).with_name("NotificationLiveTests.cmake")
    source = registration.read_text(encoding="utf-8")
    import_position = source.find(
        "find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)"
    )
    target_gate_position = source.find("if(TARGET qindaqt-shell\n")
    self.assertGreaterEqual(import_position, 0)
    self.assertGreater(target_gate_position, import_position)
```

This guard verifies:
- The import statement exists in the file
- The import occurs before the target gate that checks `TARGET KF6::GlobalAccel`
- Prevents silent-skip by construction

**Dependency check:** KF6GlobalAccel is already required by:
- `src/compositor/CMakeLists.txt` (unconditionally when KWin plugin builds)
- `src/shell/CMakeLists.txt` (when QINDAQT_BUILD_PRODUCTION_SHELL=ON)

The notification-live tests require both `TARGET qindaqt-shell` and `TARGET qindaqt_compositor`, which means KF6GlobalAccel is already available. This import does **not** widen production dependencies; it only makes an already-required dependency visible in the test scope.

**Conditional logic:** Lines 8-10 import only if qindaqt-shell exists and KF6::GlobalAccel is not yet visible:
- If shell doesn't exist, skip import (tests won't run anyway)
- If shell exists, KF6GlobalAccel should be available (because shell requires it)
- If not available, `find_package(...REQUIRED CONFIG)` fails hard, preventing silent skip
- Status:** ✓ Correct and safe

## Fresh Configuration Target Gate: Complete

**File/Line:** `tests/session/NotificationLiveTests.cmake:12-20`

All eight conditions must be satisfied for probe and test registration:

| Condition | Status | Notes |
|-----------|--------|-------|
| `TARGET qindaqt-shell` | Required | Requires QINDAQT_BUILD_PRODUCTION_SHELL=ON |
| `TARGET qindaqt-session` | Required | Requires notification/settings hierarchy |
| `TARGET qindaqt-notification-host` | Required | Requires settings/notification |
| `TARGET qindaqt-settings-service` | Required | Requires settings |
| `TARGET qindaqt-settings` | Required | Requires settings |
| `TARGET KF6::GlobalAccel` | Fixed by repair | Now visible after local import |
| `QINDAQT_BUSCTL` | Required | Must find busctl executable |
| Install paths relative | Required | KDE_INSTALL_BINDIR and PLUGINDIR must be relative |

All conditions are explicit target/variable checks. No partial registration is possible.

**Expected behavior on fresh configuration:**
- If all conditions true: probe and six rows (1080p, wuxga, 1440p, scale-125, scale-150, race-10x) register
- If any condition false: entire block skipped (no partial registration)
- **Status:** ✓ Correct and deterministic

## False-Green Paths: None Remaining

**Path 1 - Silent skip before repair:** Before import fix, the entire block would skip if KF6::GlobalAccel was not in scope. **Fixed** by local import guard.

**Path 2 - Silent skip after repair:** All eight target/variable conditions now checked. No partial registration possible. **Clear**

**Path 3 - Timeout preemption (C1):** CTest timeout of 2400s provides 600-second margin beyond sum of inner timeouts, adequate for cleanup/overhead. **Clear**

**Path 4 - Evidence false-pass:** Omar's audit verified observable evidence collection via authenticated D-Bus snapshots and explicit PID queries, not inferred from command success. **Clear**

**Path 5 - Artifact escape:** Staging validation with path containment checks prevents escape from build tree. **Clear**

**Path 6 - Process cleanup orphan:** Cleanup guard prevents leaving descendants, raises RuntimeError on survival. **Clear**

**Path 7 - Result aggregation:** Probe must emit JSON with `passed: true` and matching phase; JSON parsing fails if malformed. No result assumed from exit code. **Clear**

## What Remains Runtime-Only

Fresh Debug configuration will register probe and six rows (requires compiler lane). Cannot verify actual registration or test execution without build. The following remain runtime evidence:

- Actual probe binary and test row CTest registration in generated tree
- Test execution path and timeout behavior under load
- Nested KWin launch with real D-Bus, shell PID replacement, and process cleanup
- DND Settings1 persistence and host notification PID continuity
- Lock screen integration and unlock state machine
- Scale assertion via live Compositor1 PID and output geometry
- All observability claims from ADR-0020 private endpoint

## Recommendation

**Pass conditional on C1 and import repairs:** Both changes are correct and auditable:

1. **C1 timeout:** 2400s with 600-second margin, guarded by unit assertion requiring >= 300-second buffer
2. **KF6GlobalAccel import:** Directory-scoped target made visible before gate, guarded by unit assertion checking import-before-gate ordering
3. **No new production dependencies:** Already-required package made visible in test scope
4. **No false-green paths:** All target/variable checks explicit, process cleanup bounded, evidence authenticated

**Next action:** Await compiler lane reassignment. When reconfigured fresh, all six notification-live test rows should register. Proceed to focused Debug build and Release/sanitizer gates.
