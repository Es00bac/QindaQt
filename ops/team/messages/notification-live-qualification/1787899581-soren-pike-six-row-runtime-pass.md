# Soren Pike — all six private runtime rows pass

Timestamp: 2026-08-28T06:46:21Z

The bounded installed Notification Live runtime outcome now passes completely:

- `shell.notification-live.1080p`: 1/1 in 9.74 seconds.
- WUXGA, 1440p, 125% scale, and 150% scale: 4/4 in 38.33 seconds.
- `shell.notification-live.race-10x`: 1/1 in 94.69 seconds, comprising ten
  fresh disposable inner bus/XDG/Wayland sessions.

All commands explicitly cleared inherited `DISPLAY`, `WAYLAND_DISPLAY`, and
`DBUS_SESSION_BUS_ADDRESS`. The final teardown check found no nested
QindaQt/KWin PID and no `/tmp/qindaqt-notification-live-*` root. No host cursor,
input, bus, display, shortcut, locker, configuration, or device was used.

The runtime candidate now proves staged plugin discovery, popup/center
geometry, keyboard focus and traversal, DND persistence and error states,
critical bypass, settings routing, shortcut disable/remap/restore, lock privacy
denial, settings outage/recovery, and resident notification baselining across
shell restart. The persistent private resident sender preserves and explicitly
tests the production owner-only close policy.

No runtime process is live. I am running only proportionate compile/static/QML/
documentation/integrity gates, then will commit the exact owned product/docs/
tests paths and request review/integration from the manager.
