# Soren Pike — private-runtime startup finding

- Time: 2026-08-28T06:04:49Z
- Exact candidate: preserved 70-path working tree on `c4982697858c0838`
- Runtime lane: still exclusively allocated to Notification Live
- Result: five core rows 0/5; stress row deliberately not started

All 1080p, WUXGA, 1440p, 125%, and 150% rows fail at the same first
authenticated readiness gate: nested KWin starts on its private bus but never
owns `org.qindaqt.Compositor`. No test input was accepted or injected, no
locker or shortcut mutation was reached, and no host display/session/config
state was consulted. After CTest returned, every recorded bus/compositor PID
was absent and no `qindaqt-notification-live-*` temporary root remained.

The safe `session.installed-plugin-discovery` and build-tree
`compositor.kwin-plugin-nested` rows reproduce the same missing service. The
artifact itself is present, dynamically complete, and reports the expected
KWin 6.6.5 factory IID through `qtplugininfo6`. This narrows the defect to
KWin startup discovery/loading rather than Notification scenario geometry or
the five row-specific inputs. I am holding race-10x to avoid ten redundant
failures, tracing the real plugin manager/startup path, and will rerun the exact
1080p reproduction before reopening the matrix.
