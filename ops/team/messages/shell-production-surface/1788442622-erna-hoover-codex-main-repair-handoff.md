# Erna Hoover-Codex handoff: settle panel authority before hidden capture

- Timestamp: 2026-09-03T07:37:02-06:00
- Candidate commit: `cae66fcd5f3245c7cbea82728928d69acaee0ef1`
- Candidate tree: `1e127cb8870a101547a8f48ecfd927553600915d`
- Exact base: `22b31b94e0da12f0be54c5d0d3c48b639815e562`
- Branch: `worker/panel-visibility-main-repair`

## Mechanism and repair

The Global Menu applet did not hold a visibility lease and did not own the empty intelligent left rail. Its integrated shell timing exposed an existing observation race: the animation lease released correctly and the shell began the Wayland unmap, but KWin briefly retained the mapped/committed layer role with 0x0 geometry before removing it. The former hidden predicate only looked for a correctly sized 40-pixel panel, so it accepted that transitional role as absence and captured an unsettled inventory. The strict Python validator then correctly rejected the 0x0 record.

The repaired probe evaluates phase predicates only after the complete compositor snapshot is settled: every published surface has exact positive geometry inside the framebuffer. Hidden qualification therefore waits for authoritative role absence. The shared fakeable authority/timer waiter fails closed when a role never disappears, and the existing `desktop.virtual.panel-visibility.validator-unit` selector covers 0x0-then-absent, never-absent, and escaped-geometry sequences. The validator remains strict.

## Changed paths

- `docs/wiki/shell/panel-visibility.md`
- `tests/session/PanelVisibilityTests.cmake`
- `tests/session/panelvisibilityphasewaiter.cpp`
- `tests/session/panelvisibilityphasewaiter.h`
- `tests/session/panelvisibilitysessionprobe.cpp`
- `tests/session/run_panel_visibility_unit.cmake`
- `tests/session/tst_panelvisibilityphasewaiter.cpp`

## Acceptance evidence

- Exact Debug configure command from the worker brief: exit 0.
- Debug focused build of `qindaqt-desktop-session-probe`, all three panel-visibility producer executables, and dependencies: exit 0, 1368 actions.
- Debug focused build of the settlement/session probes: exit 0, 7 final actions after one diagnosed missing-include compile failure was repaired.
- Debug focused build of all visibility, shell-runtime, Global Menu, shell, QML-plugin, and preview targets: exit 0 (68-action and 91-action completion builds).
- `ctest --test-dir .../debug -R '^desktop\.virtual\.panel-visibility\.validator-unit$' --output-on-failure --no-tests=error -V`: exit 0, 1/1 CTest row; 3/3 Python controls and 5/5 QtTest functions passed.
- `ctest --test-dir .../debug -R '^desktop\.virtual\.sandbox-unit$' --output-on-failure --no-tests=error --parallel 1`: exit 0, 1/1 row and 114/114 Python controls passed.
- Debug `ctest` selector `^(qindaqt\.shell-visibility-|qindaqt\.shell-runtime-|qindaqt\.global-menu-|desktop\.virtual\.panel-visibility\.validator-unit$)`: final exit 0, 41/41 rows passed. An earlier dependency-discovery run reported 10 not-run/missing executables and one missing preview artifact; the listed focused builds supplied them before this final pass.
- `desktop.virtual.panel-visibility.single-wuxga`, serial with `QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop`: exit 0 twice; each invocation passed the package fixture and live row (2/2), with the row taking 47.77 s and 46.35 s.
- `desktop.virtual.panel-visibility.single-1080p`, same serial private lane: exit 0 twice; each invocation passed 2/2, with the row taking 42.30 s and 41.84 s.
- `desktop.virtual.boot.1080p`, same serial private lane: exit 0 twice; each invocation passed 2/2, with the row taking 2.01 s and 1.98 s.
- `pgrep -x kwin_wayland` before and after every nested invocation: no process found; final survivor check status 1 (empty set).
- Exact Release configure command from the worker brief: exit 0.
- Release focused visibility/runtime/Global Menu/shell build: exit 0, 794 actions; QML plugin completion build: exit 0, 12 actions.
- Release `ctest` selector `^(qindaqt\.shell-visibility-|qindaqt\.shell-runtime-|qindaqt\.global-menu-|desktop\.virtual\.panel-visibility\.validator-unit$)`: exit 0, 41/41 rows passed.
- `./tools/validate-docs`: exit 0, 132 Markdown documents plus navigation validated.
- strict MkDocs to the assigned build root: exit 0, documentation built in 1.53 s.
- `git diff --check`: exit 0.
- No JSON changed, so `python3 -m json.tool` was not applicable.
- `./tools/check-source-shape`: exit 1 because unchanged exact-base `tests/session/DesktopSessionTests.cmake` has 621 non-blank lines, exceeding 600. Direct counts are 621 in both `HEAD` and the worktree and `git diff --exit-code HEAD -- tests/session/DesktopSessionTests.cmake` is clean. That file belongs to the active desktop-stage lane and was not modified here; all added hand-written files are at most 133 physical lines.

## Bounded caveats

- This candidate repairs evidence phase settlement, not shell visibility policy: the evidence proves the production shell already releases the animation lease and ultimately removes the layer role. It deliberately makes no Global Menu production change.
- The nested evidence remains limited to the documented single-output 1080p and WUXGA software path. It does not claim fractional scaling, multi-output, GPU rendering, hardware input, or host desktop behavior.
- Repository-wide source shape cannot be reported green until the active owner decomposes the unchanged exact-base `tests/session/DesktopSessionTests.cmake` violation.

Requested next action: independent exact review then manager integration.
