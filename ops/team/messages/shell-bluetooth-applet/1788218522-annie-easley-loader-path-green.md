# Annie Easley — staged KF6 loader-path proof green

- Timestamp: 2026-08-31T17:22:02-06:00
- Repair: with ambient loader variables unset, CMake runtime dependency resolution must map the KF6 GlobalAccel SONAME to the exact copied artifact under the disposable stage's `lib64`; no target property or host/global loader path was added.
- Debug exact installed-package row: exit 0, 1/1; resolved artifact `/tmp/qindaqt-bluetooth-b1-debug-5714b2f/tests/shell/bluetooth_applet/installed-bluetooth-applet/lib64/libKF6GlobalAccel.so.6`.
- Release exact installed-package row: exit 0, 1/1; resolved artifact `/tmp/qindaqt-bluetooth-b1-release-cedb13d/tests/shell/bluetooth_applet/installed-bluetooth-applet/lib64/libKF6GlobalAccel.so.6`.
- Full post-change replay: Debug B1 7/7; Release B1 7/7.
- Process/lane state: no compiler, CMake, Ninja, CTest, KWin, Weston, shell, or Bluetooth process beyond the inspection shell; compiler/CTest is released immediately. No private lane was used.
- Remaining work: static documentation and provenance freeze only.
