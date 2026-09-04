# Tray S2 repair handoff — bounded repair of `b10692c` (degraded-state notification + acknowledgement)

- Worker: Sijue Wu (Moonshot Kimi `kimi-code/k3`, reasoning high), slug `sijue-wu`
- Exact base: `893805724933b307e165fdefc485df1ae4a13015`
- Exact product candidate: `544d1c30c2d61a5b3e12e7002ba274326dcc736c`
- Product candidate tree: `8add8cdd8f1f3377b8f413bcb4921cc07606045a`
- Product candidate parent: `82990ab8142da480885312c65d70ef9e51eb6795` (prior handoff record on top of rejected candidate `b10692c`)
- Branch/worktree: `worker/tray-applet` at `/home/cabewse/work_SPaC3/container-wm-workers/tray-applet`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/tray-applet` (system-KWin initial cache, reused per lane)

## Repair summary (answers the single P1 from `review-tray-applet-codex/verdict.md`)

1. Cause repair — `StatusNotifierMonitorAdapter::ForwardingSink` (`src/shell/status_notifier/applet/src/status_notifier_monitor_adapter.cpp`):
   notification now keys on registry **presentation truth**, not outcome acceptance. Every forwarded
   sink call emits the seam's `changed()` when the outcome was accepted OR the registry's degradation
   marker changed across the call. `registerItem()` sets that marker before returning a REJECTED
   outcome for a malformed live replacement (`malformed-item-replacement`) and for membership
   capacity overflow (`item-capacity-exceeded`), so the controller now projects `degraded`
   immediately while the registry retains the last-known-good descriptor. Pure refusals that left
   presentation untouched still never notify.
2. Acknowledgement/recovery transition at the composed boundary — smallest fail-closed option (an
   admitted controller action; no automatic recovery, so later valid traffic never silently clears a
   degradation the user was shown):
   - `StatusNotifierSourceInterface::acknowledgeDegraded()` (new pure virtual, documented contract);
   - `StatusNotifierMonitorAdapter::acknowledgeDegraded()` clears the marker via the owned registry
     and emits `changed()` only on a real transition (no-op, no notification, when not degraded);
   - `Q_INVOKABLE StatusNotifierAppletController::acknowledgeDegraded()` fails closed to a no-op
     under read denial or a missing source, otherwise forwards through the seam; the direct
     `changed()` connection reprojects synchronously back to `ready`/`empty`/`loading`.
   The choice is recorded on the status-tray page (new "Degradation acknowledgement" section).
3. Everything else unchanged: no hosting, no shell-runtime or panel QML edits, no S1 foundation or
   transport source edits, no CMake edits, no JSON edits.

## Sorted changed paths (product commit `544d1c3`)

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/status-tray.md`
- `src/shell/status_notifier/applet/include/qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h`
- `src/shell/status_notifier/applet/include/qindaqt/shell/status_notifier/applet/status_notifier_monitor_adapter.h`
- `src/shell/status_notifier/applet/include/qindaqt/shell/status_notifier/applet/status_notifier_source_interface.h`
- `src/shell/status_notifier/applet/src/status_notifier_applet_controller.cpp`
- `src/shell/status_notifier/applet/src/status_notifier_monitor_adapter.cpp`
- `tests/shell/status_notifier/applet/installed_cpp_consumer.cpp`
- `tests/shell/status_notifier/applet/status_notifier_applet_test_fakes.h`
- `tests/shell/status_notifier/applet/tst_status_notifier_applet_adapter.cpp`
- `tests/shell/status_notifier/applet/tst_status_notifier_applet_controller.cpp`

Coordination-only paths (separate commit): this handoff, `ops/team/workers/sijue-wu.md`.

## New registered rows (in existing executables)

- `qindaqt.status-notifier-applet-adapter` gains
  `malformedReplacementNotifiesDegradedImmediatelyAndAcknowledges` (real watcher/monitor/registry
  over a private bus; a live item publishes a C0-control-character title replacement) and
  `capacityRejectionNotifiesDegradedImmediately` (a scripted over-advertising watcher — the
  production watcher service caps its own inventory at kMaxItems, so only a broken/hostile watcher
  can drive the registry's membership-overflow arm; 65th live registration post-population).
- `qindaqt.status-notifier-applet-controller` gains
  `degradedProjectionAndAcknowledgementFlowThroughTheSeam` and
  `acknowledgeDegradedFailsClosedWithoutObservation`.

Negative control actually executed: with only the sink condition reverted to `accepted()`-only
(uncommitted scratch edit), both new adapter rows FAILED with `controller.phaseText()` stuck at
`"ready"` (the exact b10692c defect, 2 failed / 2 passed, Debug); the file was then restored with
`git checkout` and the rows pass again. This matches the reviewer's reproduction on the exact
candidate.

## Verification evidence (every command run by me on this candidate; exit 0 unless stated)

Builds (`cmake --build <ROOT>/<profile> --parallel 3 --target ...`, strict warnings): Debug built
all status-notifier test executables, the six applet-integrity test executables, `qindaqt-shell`,
`qindaqt-shell-preview`, `qindaqt-desktop-session-probe`,
`qindaqt_controls_font_pinning_tests`, and the Controls/GlobalMenu/Launcher QML plugins — exit 0.
Release built the same set — exit 0.

Test rows, both profiles, under
`env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`
with `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1`:

- `ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.(status-notifier-|applet)' --output-on-failure --no-tests=error`
  — Debug 21/21 pass, Release 21/21 pass (15 status-notifier rows incl. the repaired adapter and
  controller rows, 6 applet integrity rows).
- `-R '^qindaqt\.shell-runtime-component-closure$'` — Debug 1/1, Release 1/1.
- `-R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)'` — Debug 3/3, Release 3/3.
  No nested, host-bus, hardware, uinput, or network row was run.

Static gates (worktree root): `./tools/validate-docs` exit 0 (139 documents);
`/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`
exit 0; `./tools/check-source-shape` exit 0 (nine non-fatal pre-existing decomposition-review
warnings, none candidate-touched beyond the already-known `src/shell/CMakeLists.txt` 543-line one,
unchanged by this repair); `git diff --check` exit 0. No JSON file changed in this repair, so the
`json.tool` gate had no inputs. Standalone boundary gate:
`cmake -DQINDAQT_STATUS_NOTIFIER_APPLET_SOURCE_DIR=... -DQINDAQT_STATUS_NOTIFIER_APPLET_POISON_DIRECTORY=<ROOT>/boundary-poison -P tests/shell/status_notifier/applet/check_status_notifier_applet_boundary.cmake`
exit 0 (11 files validated, poison probes rejected).

## Bounded caveats (deliberately not claimed)

- The QML module exposes the acknowledgement only as the controller's `Q_INVOKABLE`; no new QML
  affordance (button/menu entry) was added — none was required by the contract, and panel/hosting
  QML stays untouched for the hosting lane.
- The capacity arm is driven through a scripted over-advertising watcher because the production
  watcher service enforces kMaxItems before the monitor/registry can see a 65th item; the registry
  capacity path remains defense-in-depth against broken/hostile watchers.
- All prior candidate caveats stand: no panel hosting or dispatcher composition, no dbusmenu entry
  activation, no host session bus evidence, no assistive-technology bridge, QML rows depend on the
  pre-existing pinned-font fixture.

Requested next action: independent exact review (Rózsa Péter recheck) then manager integration.
