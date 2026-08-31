# S3 secondary interaction mode runs but no qualified WL-1 surface maps

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T03:38:15-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: stopped at first new dual failure; final gates not started

The authorized tests/session-only probe repair is implemented without changing
the primary mode or acceptance contract:

- `--open-notification-center-secondary` is now an exact recognized mode;
- it queries the private compositor output inventory and requires exactly one
  valid `WL-1` geometry;
- it moves only the private development pointer to the center of `WL-1`, then
  sends the unchanged four Meta+N key transitions (five events total);
- it emits success only for one mapped, committed, active 440x640 notification
  surface whose desired and actual output names are both `WL-1`;
- unknown and extra arguments remain fail-closed.

Focused CLI positive/hostile tests pass 2/2; the registered focused CTest passes
1/1; full contained-session passes 95/95; `git diff --check` passes; and the
focused probe build is green. Probe SHA-256 is `230dcf0f…`.

The one authorized serialized dual rerun executed only its package fixture and
registered row. Package setup passed. The selector again exited 0, the repaired
secondary probe was recognized, and all five events were accepted; it then
exhausted the existing three-second surface observation loop and returned:

```text
notification center did not map on the private seat
```

Preserved result:

`/tmp/qindaqt-s3-selene-build/tests/session/desktop-session-results/3467cc710acda0b81c606342939859ea`

- result JSON SHA-256 `3cbe8d3e…`, outcome failure, return code 1;
- sandbox command SHA-256 `df2cc531…`;
- selector log SHA-256 `e7864e01…`;
- interaction log SHA-256 `0faba721…`;
- full CTest log
  `/tmp/qindaqt-s3-selene-build/qt-private-dual-1080p-secondary-repaired.log`,
  SHA-256 `10d8acf5…`; row failed at 4.24 seconds.

The last pre-injection readiness probe proves the exact horizontal dual
1920x1080 topology, priorities, two mapped per-output docks, required
applications/services, and private input. The current interaction probe does
not archive its polled post-injection inventories, so preserved evidence cannot
distinguish no notification surface from a surface that mapped on `WL-0` and
was correctly rejected by the new exact `WL-1` filter. I therefore made no
inference and no further diagnostic/product/harness change.

Fresh executable inspection finds zero owned KWin, Weston, staged QindaQt,
probe, or selector survivors. No further runtime, repeatability, docs/source,
provenance/clean gate, or candidate commit started. The exact next action needs
manager routing: authorize a bounded post-injection surface-inventory capture
to distinguish shell output selection from action delivery, or route the
secondary-output shell behavior as a product defect.
