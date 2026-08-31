# S3 post-delivery 1080p150 material diagnosis

- Worker: Dorothy Vaughan
- Update: 2026-08-31T12:34:42-06:00
- Coordination base: `39c83a23143f36c3bfec8686b7cbc0121ea8a418`
- Execution state: read-only diagnosis complete; executable lane released
- Requested action: authorize the bounded S3 test-owned readiness repair below

## Preserved evidence

The comparison label is important: green `84bb6749…` is the current
1440p125/unity-inspired/dusk row. The actual same-row control for red
`2c6dd8f5…` is prior green `09f38a9e…`, also
1080p150/mate-inspired/dark.

Red `2c6dd8f5…` reached exact compositor topology at `session-probe-004`: two
committed/mapped dock surfaces owned by shell PID 53, both still reporting
zero-sized committed geometry. It then returned interaction 8 after the exact
`qindaqt-shell` / `qindaqt_toggle_notification_center` press and release had
completed, but no center surface mapped. Prior same-row green `09f38a9e…` had
no shell surfaces at probe-004 and did not accept readiness until probe-005,
about 277 ms later, with both docks at their settled geometry; its sole Meta+N
mapped the center. Current green `84bb6749…` has the same one-probe-later shape
and exact activation marker. This timing correlation does not retrospectively
record the red shell's privacy bit, so I do not claim a captured value that is
absent.

The source path nevertheless identifies one exact prerequisite that the S3
probe does not observe:

1. `ShellRuntimeApplication` initializes notification privacy from
   `SessionLockStateMonitor::contentMayBeShown()`, whose fail-closed initial
   state is `Unknown` and therefore false.
2. `SessionLockStateMonitor::start()` merely begins asynchronous owner/PID/
   lock-state authority probes and returns.
3. Shell startup then constructs and registers the KGlobalAccel action before
   initial surface reconciliation completes.
4. A valid action delivery calls `NotificationCenterAppletAccess::toggle()`,
   which returns without emitting when `privatePresentationAllowed` is false;
   `NotificationPresentationController::toggleCenter()` independently has the
   same fail-closed return.

Therefore exact registry metadata, active component state, and even exact
KGlobalAccel press/release can all be true while the production shell still
lawfully discards the toggle. This is the smallest evidence-consistent cause of
the observed return-8 branch. It is a missing harness prerequisite, not a case
for delaying or retrying input and not evidence of a production privacy defect.

## Existing public boundary

The production shell already publishes the read-only, development-only
`org.qindaqt.ShellDevelopment1.Snapshot()` after initial shortcut and surface
reconciliation. Its schema-1 object includes `shellPid`,
`presentation.privatePresentationAllowed`, `presentation.centerOpen`, and
center-window existence, visibility, geometry, and `outputName`. Notification
Live authenticates the service and each snapshot to the externally observed
shell PID and already awaits `privatePresentationAllowed == true` before its
first input. S3 currently observes only compositor topology and KGlobalAccel;
it never consumes this documented shell-ready boundary.

## Proposed bounded repair

Authorize only S3/session harness and testing-harness documentation changes:

- add a cohesive `tests/session/desktopnotificationshellreadiness.{h,cpp}`
  pure schema/predicate boundary and
  `tests/session/tst_desktopnotificationshellreadiness.cpp` mutations;
- update `tests/session/desktopsessionprobe.cpp` to use that helper for one
  non-polling ShellDevelopment sample per regular probe: derive the unique shell
  PID from authenticated dock surfaces, authenticate the service and schema-1
  snapshot to that PID, and emit a retryable pending state until privacy is
  allowed, `centerOpen == false`, and an existing hidden center window is on
  the selected output;
- add that evidence to `tests/session/desktop_session_readiness.py` so the
  existing outer 15-second readiness poll retries a cold/pending shell sample,
  with Python mutations in
  `tests/session/test_desktop_session_readiness_unit.py` and fixture updates as
  required; do not put a 7.5-second client poll inside the fixed one-second
  regular probe;
- synchronously sample and validate the same exact owner/PID/state in
  `tests/session/desktopsessionprobe.cpp` immediately before its sole Meta+N
  batch;
- make the smallest additive `tests/session/DesktopSessionTests.cmake`
  source/unit target registration for the extracted helper;
- after that same batch, require shell `centerOpen == true`, visible center,
  and an increased `centerOpenedCount` beside the existing exact KGlobalAccel
  signal and compositor-surface proof, so any future return diagnoses the
  action-to-presentation boundary directly;
- document this timing-independent prerequisite in
  `docs/wiki/development/testing-harness.md`.

Focused mutations should reject invalid schema, wrong/missing shell PID,
privacy denied, pre-open center, absent center window, prematurely visible
center, wrong output, unchanged open counter, and post-input center/window
disagreement. Preserve one input batch: no fixed sleep, warm-up input, target
input retry, direct action invocation, production/profile/Network edit, or
broader path.

No configure, compiler, CTest, private bus, nested runtime, production edit,
or new input ran during this diagnosis. Red `2c6dd8f5…`, current green
`84bb6749…`, and prior same-row green `09f38a9e…` remain unchanged.
