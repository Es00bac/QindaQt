Audio applet A1 third descendant rereview verdict (Astra Quill)

Exact repair descendant candidate `84712fa7c2a4542bf2c62ba98b2fc5b5f32b73f4` failed exact evaluation with 1 P1 defect (uncovered by test execution).

Verification results:
- Confined external CMake build with `CMAKE_AUTOMOC_PATH_PREFIX=ON`: Passed.
- Pointer syntax test compilation repair: Passed.
- `python3 tools/check-source-shape --root . --warnings-as-errors`: Passed.
- `python3 tools/validate-docs --root .`: Passed.
- `git diff --check`: Passed.
- `qindaqt_audio_applet_model_tests`: 12 passed, 0 failed.
- `qindaqt_audio_applet_controller_tests`: 14 passed, 1 failed.

Concrete P1 defect:
Test `AudioAppletControllerTests::unavailableSnapshotFailsClosedWithReason` fails because `m_controller->deviceRows().isEmpty()` evaluates to false. 

Investigation:
The test pushes an `Availability::Unavailable` snapshot that contains active rows. According to the contract implied by the test and the "Fails Closed" invariant, if the phase is `Unavailable` (or `Loading`), the projection should clear out devices and streams. 
However, in `AudioAppletController::reproject()` (`audio_applet_controller.cpp`), an unavailable snapshot is passed directly to `AudioAppletModel::project()`. In `AudioAppletModel::project()` (`audio_applet_model.cpp`), there is no early return to clear rows for `Phase::Unavailable` (unlike the check for `!snapshot->wireValid`). Thus, rows are erroneously populated.

Precise repair route:
In `src/shell/audio_applet/audio_applet_model.cpp` inside `AudioAppletModel::project()`, add an early return (returning a model with no rows) if `phase == Phase::Unavailable || phase == Phase::Loading`. Alternatively, in `src/shell/audio_applet/audio_applet_controller.cpp` inside `reproject()`, pass `nullptr` for the snapshot parameter when `phase` is not `Phase::Ready` or `Phase::Degraded`. Either fix satisfies the Fails Closed contract required by the test.

P0/P1/P2/P3 = 0/1/0/0.

Bounded caveats: QML is unlinted and unrendered in this slice; default selection/stream moves excluded; retry-on-degraded absent.

Next action: Rune Mercer must repair this product logic defect to align with the Fails Closed contract.

— Astra Quill, 2026-08-28T12:50:49-06:00
