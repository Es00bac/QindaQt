# S3 dual rerun passes selector and stops at unsupported interaction mode

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T03:30:38-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: stopped at first post-selector failure; no later row or final gate

The bounded selector repair passes its full pre-runtime gate: contained-session
units are 95/95 (`dual-selector-full-contained.log`, SHA-256 `fe5a110a…`) and
`git diff --check` is clean. The one authorized serialized rerun executed only
the package setup fixture and registered dual row. Setup passed; the exact
`/usr/bin/kscreen-doctor output.WL-1.primary` child exited 0 under its repaired
private-first plugin environment, and the row produced a fifth complete
readiness probe after that selector. This closes the prior timeout as repaired.

The row then stopped before interaction/capture with:

```text
desktop session qualification failed: private-seat interaction did not return exact evidence
```

Preserved result:

`/tmp/qindaqt-s3-selene-build/tests/session/desktop-session-results/934cf2155eb38ddba40a4ca9513f6283`

- result JSON SHA-256 `1eab2219…`, outcome failure, return code 1;
- sandbox command SHA-256 `b8035e04…`;
- selector log SHA-256 `e7864e01…` (only the known fontconfig warning);
- interaction log SHA-256 `4a1e911a…`, exact text `unsupported probe arguments`;
- probe 005 SHA-256 `ad5bff88…`, with both exact 1920x1080 outputs,
  horizontal geometry, mapped full-height per-output docks, required
  apps/services, and private input;
- full CTest log
  `/tmp/qindaqt-s3-selene-build/qt-private-dual-1080p-repaired.log`, SHA-256
  `61a014bd…`; fixture passed and row failed at 1.51 seconds.

The source mismatch is exact: `desktop_session_runtime._run_interaction()`
passes `--open-notification-center-secondary` for the dual scenario, while
`desktopsessionprobe.cpp` recognizes only `--open-notification-center`; every
other nonempty argument path prints `unsupported probe arguments` and returns
2. Therefore no interaction marker can exist. The final validator already
requires the strong dual contract (five injected events, notification surface
mapped/active on `WL-1`, exact desired/output agreement), so this is a missing
probe implementation rather than a vacuous assertion.

The earlier sampler repair remains green: focused 6/6, full 93/93 at that
checkpoint, and repaired 1080p150/dark result
`1b30bfe610d96520125119ffaefdaf8b` authenticates exact scale/theme,
apps/docks/interaction, PSS, containment, and empty teardown.

Fresh executable inspection finds zero owned KWin, Weston, staged QindaQt,
probe, or selector survivors. Per the serial matrix contract, no additional
repair, nested runtime, capture, repeatability gate, docs/final gate, or
candidate commit was started. Please authorize the smallest exact
tests/session probe implementation for the already-required secondary-output
interaction, with focused regressions and one dual rerun, or route it for
repair.
