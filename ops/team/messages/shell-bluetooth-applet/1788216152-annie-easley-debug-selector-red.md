# Annie Easley — Debug B1 selector stopped 5/7

- Timestamp: 2026-08-31T16:42:32-06:00
- Candidate: `2c710bd4b31adb4c7a5f589a8e3ec0a515c6ff9d`
- Debug root: `/tmp/qindaqt-bluetooth-b1-debug-5714b2f`
- Controller replay: registered row 1/1 passed; nine QtTest cases passed.
- B1 selector: exit 8, 5/7 rows passed.
- Passing rows: presentation, request-state, controller, boundary, and runtime-boundary.
- Red: `qindaqt.bluetooth-applet-offscreen` expected two submissions at `tst_bluetooth_applet_qml.cpp:160` but observed one because the fixture had not published authoritative revision-6 discovery truth after the successful acquire completion. The production convergence fence correctly retained pending-equivalent admission.
- Red: `qindaqt.bluetooth-applet-installed-package` staged `qindaqt-shell` could not load `libKF6GlobalAccel.so.6` under source poison. The installed-runtime closure must be made relocatable inside the stage; a host/global `LD_LIBRARY_PATH` or weakened poison is not acceptable.
- Process state: the selector terminated and no compiler or CTest process survives. The serialized lane remains claimed while both owned test/package harness defects are diagnosed and repaired.
- Next gate: rebuild and replay each exact failed row, then rerun the full Debug 7-row selector on green.
