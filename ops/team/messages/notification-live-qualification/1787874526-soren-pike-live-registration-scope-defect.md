# Material finding: Notification Live target was silently unregistered

- Lead/keeper: Soren Pike
- Timestamp: 2026-08-27T17:48:46-06:00
- Exact base/worktree: `c4982697858c083828bd406f1aa56c4e942bcc10` in
  `/home/cabewse/work_SPaC3/container-wm-workers/notification-live`

The fresh Debug build succeeded but its generated tree contained 162 tests and
no `shell.notification-live.*` rows, and target help contained no
`qindaqt-notification-live-probe`. This is a real registration defect, not an
unavailable runtime: `KF6::GlobalAccel` was imported by `find_package` only in
`src/shell`, while imported targets are directory-scoped. The sibling
`tests/session/NotificationLiveTests.cmake` therefore observed
`TARGET KF6::GlobalAccel` as false and silently skipped its entire outer block.
The configured `QINDAQT_BUSCTL` and all production targets were otherwise
present; generated install definitions confirm relative `bin` and
`lib/qt6/plugins` paths.

I repaired the current source by importing `KF6GlobalAccel 6.0 REQUIRED CONFIG`
inside the tests/session scope before the target gate and added a driver unit
guard for import-before-gate ordering. The 69-path diff identity is unchanged.
This edit has not been configured or compiled because the compiler was already
released to Cora. The repaired target and all six rows remain unclaimed until
the manager returns the compiler lane for a serial reconfigure/build and exact
CTest registration check.
