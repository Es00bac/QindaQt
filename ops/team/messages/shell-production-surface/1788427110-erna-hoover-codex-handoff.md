# Erna Hoover-Codex: panel visibility production-proof handoff

- Timestamp: 2026-09-03T03:18:30-06:00
- Status: handoff
- Feature: QQ-004.02 Window-aware hiding, reveal/hold, and dynamic reservation
- Exact candidate commit: `a52561f6accaa2cd43e20ba7251e446d8c5ae1ad`
- Candidate tree: `27d3f5fa82cac65d9e6e90e07fc835be9d8b2d78`
- Exact base: `349f805b685c0b5b1ad146d600dc1c3fa528281d`
- Requested next action: independent exact review then manager integration

## Outcome

The production shell now owns pointer containment and one-pixel edge reveal,
output-scoped notification/applet popup holds, reduced-motion-aware opacity
transitions, and the stable `qindaqt_reveal_panels` `Meta+Space` action. Every
producer acquires the existing move-only interaction leases; mapping,
reservation, window inventory, and policy remain behind their existing
boundaries. Rejected compositor authority cancels transitions, restores full
opacity, and retains safe-visible behavior.

The installed private-desktop proof adds serial 1920x1080 and 1920x1200 rows.
Each maps a real painted client; proves overlap hide, private-seat Meta-drag
restore, post-close restore, edge and shortcut reveal, notification-center
hold/close, a mapped/reserving `never` panel in every phase, six validated
private-parent framebuffer captures, and authenticated survivor-free teardown.

## Changed paths

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/panel-visibility.md`
- `src/shell/CMakeLists.txt`
- `src/shell/runtime/panelvisibilityanimation.cpp`
- `src/shell/runtime/panelvisibilityanimation.h`
- `src/shell/runtime/panelvisibilitypointer.cpp`
- `src/shell/runtime/panelvisibilitypointer.h`
- `src/shell/runtime/panelvisibilitypopup.cpp`
- `src/shell/runtime/panelvisibilitypopup.h`
- `src/shell/runtime/panelvisibilityruntime.cpp`
- `src/shell/runtime/panelvisibilityruntime.h`
- `src/shell/runtime/panelvisibilityshortcut.cpp`
- `src/shell/runtime/panelvisibilityshortcut.h`
- `src/shell/runtime/panelvisibilitytimer.cpp`
- `src/shell/runtime/panelvisibilitytimer.h`
- `src/shell/runtime/shellruntimeapplication.cpp`
- `src/shell/runtime/shellruntimeapplication.h`
- `tests/CMakeLists.txt`
- `tests/session/CMakeLists.txt`
- `tests/session/PanelVisibilityTests.cmake`
- `tests/session/fixtures/panel_visibility_profiles/panel-visibility-proof.json`
- `tests/session/panelvisibilitysessionprobe.cpp`
- `tests/session/test_panel_visibility_nested.py`
- `tests/shell_visibility_producers/CMakeLists.txt`
- `tests/shell_visibility_producers/tst_panelvisibilityproducers.cpp`

## Acceptance evidence

All commands ran from the candidate worktree unless the command names an
explicit build directory.

- Exact brief Debug configure command with build root
  `/home/cabewse/work_SPaC3/builds/qindaqt/panel-visibility-proof/debug` and
  `qindaqt-665-initial-cache.cmake`: exit 0.
- Exact brief Release configure command with build root
  `/home/cabewse/work_SPaC3/builds/qindaqt/panel-visibility-proof/release` and
  `qindaqt-665-initial-cache.cmake`: exit 0.
- `cmake --build .../debug --parallel 3 --target qindaqt_shell_panel_visibility_producers qindaqt-shell qindaqt_panel_visibility_producer_tests qindaqt-panel-visibility-session-probe`: exit 0.
- `cmake --build .../debug --parallel 3 --target qindaqt_panel_visibility_modes_tests qindaqt_panel_visibility_scope_tests qindaqt_panel_visibility_validation_tests qindaqt_compositor_visibility_snapshot_tests qindaqt_compositor_visibility_state_tests qindaqt_compositor_visibility_wire_roundtrip_tests qindaqt_compositor_visibility_client_tests qindaqt_qt_compositor_visibility_transport_tests qindaqt_panel_interaction_store_tests qindaqt_panel_visibility_inventory_assembler_tests qindaqt_panel_runtime_plan_assembler_tests qindaqt_output_inventory_matcher_tests qindaqt_shell_runtime_options_tests`: exit 0.
- `ctest --test-dir .../debug -R '^qindaqt\.(shell-visibility|shell-orchestration-(interactions|visibility-inventory|runtime-plan|output-match)|shell-runtime-(options|catalog))' --output-on-failure --no-tests=error`: exit 0, 20/20 passed.
- `ctest --test-dir .../debug -R '^desktop\.virtual\.sandbox-unit$' --output-on-failure --no-tests=error`: exit 0, 1/1 passed before live rows.
- `QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop ctest --test-dir .../debug -R '^desktop\.virtual\.panel-visibility\.(single-1080p|single-wuxga)$' --output-on-failure --no-tests=error`: exit 0, 3/3 passed (package fixture plus two serial installed interaction rows); final row times 32.08 and 34.87 seconds.
- `cmake --build .../release --parallel 3 --target qindaqt_shell_panel_visibility_producers qindaqt-shell qindaqt_panel_visibility_producer_tests` plus the same dependency-adjacent unit targets named above: exit 0.
- `ctest --test-dir .../release -R '^qindaqt\.(shell-visibility|shell-orchestration-(interactions|visibility-inventory|runtime-plan|output-match)|shell-runtime-(options|catalog))' --output-on-failure --no-tests=error`: exit 0, 20/20 passed.
- `./tools/validate-docs`: exit 0, 127 Markdown documents/navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/panel-visibility-proof/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 2059 files checked; only pre-existing decomposition-review warnings outside this lane remained.
- `git diff --check`: exit 0.
- `python3 -m json.tool tests/session/fixtures/panel_visibility_profiles/panel-visibility-proof.json > /dev/null`: exit 0.

Earlier exploratory nested attempts failed closed while establishing the
installed launcher import closure, correcting the Wayland move-away gesture,
and replacing a non-opening generic applet click with the authenticated
notification-center popup. None is acceptance evidence; the final command
above was rerun after the last product and CMake changes and passed completely.

## Bounded caveats

- The candidate does not qualify fractional scaling, multi-output interaction,
  GPU/OpenGL rendering, physical input, visual screenshot baselines, popup
  placement aesthetics, or profiles/themes beyond the dedicated proof fixture.
- The live nested popup phase exercises the production notification center;
  output-scoped launcher/power QML popup discovery is covered through the same
  producer boundary and focused fake-store row, not a separate live click row.
- No host desktop, host D-Bus service, host input node, uinput device, hardware,
  network call, or non-private session row was used.
