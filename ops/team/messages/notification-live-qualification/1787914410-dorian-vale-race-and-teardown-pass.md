# Dorian Vale — fresh race and teardown gates pass

- **Timestamp:** 2026-08-28T10:53:30Z
- **Exact candidate:** `557260a50faaf083733afe5972ad6541ef398108`
- **Tree:** `8f9f131461157b33bb88e0b4a46811e2308c9329`
- **Status:** working; assembling the terminal immutable verdict

The exact candidate's installed `shell.notification-live.race-10x` test passed
in 93.49 seconds. Its preserved JSON reports `passed: true`,
`repetitions: 10`, and ten independent runs; every run reports its own fresh
compositor, initial shell, replacement shell, notification host and settings
PIDs, `freshShellPid: true`, `hostPidContinuous: true`, all six phases passed,
and `freshShellAuthentication: true` in the final shell-restart phase.

Post-test containment checks are clean:

- zero `/tmp/qindaqt-notification-live-*` runtime roots remain;
- no process command line references the detached review worktree or any of
  its Notification Live staging roots;
- the freshly staged Debug compositor plugin has no `ldd -r` not-found or
  undefined-symbol diagnostics;
- its dynamic symbols contain the exported KWin `LayerSurfaceV1Interface` and
  `WaylandServer::findWindow` API use, and no `LayerShellV1Window` dependency;
- the exact detached candidate remains product-clean; only ignored build
  output exists.

There is no blocking finding. I am completing the exact path/line evidence
ledger and will issue one terminal P0–P3 verdict and QQ-004.05 maturity decision
without rerunning or widening the accepted runtime scope.
