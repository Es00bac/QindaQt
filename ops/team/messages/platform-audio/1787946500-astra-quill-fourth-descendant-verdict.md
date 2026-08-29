Audio applet A1 fourth descendant rereview verdict (Astra Quill)

Exact repair descendant candidate `14abe57028227ac5f2d152bfe062a01fdafaded1` exactly passed evaluation. 

Verification results:
- Confined external CMake build with `CMAKE_AUTOMOC_PATH_PREFIX=ON`: Passed (27 targets built).
- Pointer syntax test compilation repair: verified in previous run, persists.
- `python3 tools/check-source-shape --root . --warnings-as-errors`: Passed (1013 files).
- `python3 tools/validate-docs --root .`: Passed (64 files).
- `git diff --check`: Passed (no violations).
- `qindaqt_audio_applet_model_tests`: 12 passed, 0 failed.
- `qindaqt_audio_applet_controller_tests`: 15 passed, 0 failed.

Repairs verification:
The exact logic failing the `unavailableSnapshotFailsClosedWithReason()` test was repaired in `AudioAppletModel::project()` where an early return now successfully clears rows when phase is `Phase::Unavailable` or `Phase::Loading`. Both the unit tests and the intent behavior confirm that Degraded and Ready phases keep rows, while Loading and Unavailable appropriately exhibit Fails Closed properties with zero rows. 

P0/P1/P2/P3 = 0/0/0/0.

Bounded caveats: QML is unlinted and unrendered in this slice; default selection/stream moves intentionally excluded; retry-on-degraded absent.

Next action: The manager must integrate this final passing candidate.

— Astra Quill, 2026-08-28T13:01:42-06:00
