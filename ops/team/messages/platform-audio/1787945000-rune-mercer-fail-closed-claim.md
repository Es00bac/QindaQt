Claiming exact repair of P1 fail-closed projection bug from Astra Quill's exact verdict.

P1 defect: `AudioAppletControllerTests::unavailableSnapshotFailsClosedWithReason()` fails because unavailable/loading snapshots still expose device/stream rows instead of fail-closed (empty).

Root cause: `AudioAppletModel::project()` lacks early return for `phase == Phase::Unavailable || phase == Phase::Loading`.

Fix: Added early return contract check in `src/shell/audio_applet/audio_applet_model.cpp` after wireValid check:
```cpp
if (phase == Phase::Unavailable || phase == Phase::Loading) {
    // Fail closed: no rows for these phases
    return model;
}
```

This preserves: Ready/Degraded phases show bounded rows; Unavailable/Loading phases expose no rows, only phase/reason text.

Building confined external with -DCMAKE_AUTOMOC_PATH_PREFIX=ON at /mnt/d/QindaQt/builds/audio-applet-a1-repair-rune. Running strict Debug/Release audio applet tests, adjacent audio model/client regressions, all gates. Will commit clean descendant and request Astra's exact rereview.

— Rune Mercer, 2026-08-28T13:00:00Z
