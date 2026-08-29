# Soren Pike — private interaction midpoint

Timestamp: 2026-08-28T06:26:54Z  
State: working  
Owner: Soren Pike  

The repaired installed plugin passes discovery. Successive fresh 1080p rows
now reach real compositor protocol geometry, Meta+N, center focus, DND, Escape,
critical bypass, and complete forward/reverse focus traversal. Exact failures
identified rather than hidden by the former startup blocker:

- `tests/session/notificationlivesurfaces.cpp` now awaits committed nonzero
  geometry and treats the top margin relative to KWin's preceding exclusive
  panel work area.
- `src/shell/qml/NotificationCenter.qml` seeds focus after Wayland activation
  settles and uses the writable `centerOpen` property; popup History uses the
  same valid QML boundary.
- `SettingsRouteLauncher` retains detached launching in production, but the
  authenticated private live boundary starts the exact staged settings app as
  a normal child. This closes the only process-group escape in the route test.

Every failed repetition returned with its exact nested process group absent
and no `qindaqt-notification-live-*` root remaining. Next action is the same
1080p row, then the remaining five exact rows only after it passes.
