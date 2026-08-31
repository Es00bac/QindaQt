# S3 diagnostic proves shell notification output-selection product defect

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T03:46:40-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: exact product boundary identified; stopped before product repair

I first inspected every process, probe, sandbox, and result log in preserved
result `3467cc710acda0b81c606342939859ea`. None contained a post-injection
inventory, so the prior archive could not distinguish an absent notification
surface from a correctly rejected `WL-0` surface.

The authorized instrumentation changes only the failure path after the
interaction probe has already returned its unchanged verdict. It launches one
ordinary no-argument snapshot and archives:

- the original interaction argument/exit and requested output;
- compositor output inventory;
- shell visibility and development-surface inventories;
- all required service owners/PIDs, including Notifications.

It changes no selector, five-event sequence, three-second observation window,
surface predicate, topology, product code, timeout, or pass/fail behavior. The
focused contract suite passes 23/23, full contained-session passes 96/96, and
`git diff --check` passes. A regression proves the separate failure artifact is
created with outputs/surfaces/visibility, notification PID, and `WL-1` target.

The sole authorized diagnostic dual rerun preserved:

`/tmp/qindaqt-s3-selene-build/tests/session/desktop-session-results/a547453211a4477f93363522c7583d4b`

Exact evidence in `logs/session-interaction-failure.log` (SHA-256
`3170cb49…`) is decisive:

- the secondary interaction returns 8 only because no qualifying `WL-1`
  surface is observed;
- the notification action did succeed: shell PID 59 has one 440x640,
  mapped/committed/active notification-center surface;
- that surface reports both `desiredOutputName: WL-0` and
  `outputName: WL-0`, at `(1464,16)`, the first output's right edge;
- `ShellVisibilitySnapshot` likewise records the active shell window on
  `WL-0`;
- simultaneously, compositor output generation 2 records `WL-1` at priority 1
  and the demoted `WL-0` at priority 2, proving the primary transfer completed;
- `org.freedesktop.Notifications` remains healthy and owned by PID 56, so the
  notification host is not the routing failure; shell PID 59 owns the surface.

Result JSON SHA-256 is `4be077de…`; sandbox command is unchanged; full CTest
log `/tmp/qindaqt-s3-selene-build/qt-private-dual-1080p-diagnostic.log` is
`2aa5df43…`. Package setup passed, the row failed at 6.53 seconds, and fresh
executable inspection finds zero owned survivors.

The exact product boundary is shell runtime output selection:

- `src/shell/runtime/shellruntimeapplication.cpp:458` always passes
  `m_application.primaryScreen()` to the notification controller, even though
  the compositor visibility client has already delivered generation 2 and
  triggered reconciliation;
- `src/shell/runtime/notificationwindowcontroller.cpp:182` retains/recreates
  notification windows on that `QScreen`;
- `src/shell/runtime/notificationwindowcontroller.cpp:293` and `:296`
  configure popup and center layer surfaces on it.

Reproduction: start the registered horizontal dual row; wait for exact dual
readiness; run `kscreen-doctor output.WL-1.primary`; inject the private pointer
to the center of `WL-1` followed by Meta+N. KWin publishes generation 2 with
`WL-1` priority 1, yet qindaqt-shell requests and receives the center on
`WL-0`.

This is not an owned harness-observation defect, so I propose no second harness
semantic change. It requires a separately owned shell-runtime repair that
chooses the notification output from authoritative live output/seat policy
rather than the stale Qt primary screen, while preserving the exact dual-row
assertion. Per instruction I stopped after this diagnostic run: no product
edit, rerun, final gate, docs update, or candidate commit followed.
