# Sela North — Power applet P1 compiled-repair handoff

- Time: 2026-08-28T11:32:30-06:00
- Owner: Sela North, Power applet P1 compiled-repair implementer (Google Antigravity Vertex ADC `gemini-3.7-flash-high`, reasoning high)
- State: complete; clean descendant commit ready for independent cross-provider review
- **Exact descendant commit: `d11a69d36c30d5100c3878fd0fa505c792ad1c6b`**
- Base: `251c62065dcbc393c3d4067858bf28329f1f881d` (Mara Voss P1 candidate preserved by Manager Maya Frost)
- Branch/worktree: `worker/power-applet-p1-repair-sela` at `/home/cabewse/work_SPaC3/container-wm-workers/power-applet-p1-repair-sela`, clean tree
- Requested reviewer: independent Claude or GLM reviewer (cross-provider verification required for Gemini implementation)

## Summary of repairs

1. **CMake / AUTOMOC wiring**: Added `add_subdirectory(shell/power_applet)` under `QINDAQT_BUILD_SHELL` in both `src/CMakeLists.txt` and `tests/CMakeLists.txt`.
2. **Namespace alias repairs in test units**: Added `namespace Power = QindaQt::Power;` and `namespace Brightness = QindaQt::Brightness;` to `tst_power_applet_presentation.cpp`, `tst_power_applet_controls.cpp`, and `tst_brightness_request_state.cpp` to resolve unqualified `Power::` and `Brightness::` type references under QtTest.
3. **Defect repair in presentation projection**: Updated `projectSupply` in `power_applet_presentation.cpp` to accept `bool *degraded` and propagate `*degraded = true` when encountering a power supply with an invalid handle (`!supply.handle.isValid()`), aligning implementation with the documented contract that invalid supply handles degrade the model phase.
4. **Documentation & test matrix registration**:
   - Added `src/shell/power_applet` source ownership row to `docs/wiki/architecture/module-boundaries.md`.
   - Added `qindaqt.power-applet-` selector row to `docs/wiki/development/testing-harness.md`.
   - Updated `docs/wiki/shell/power-applet.md` maturity to compiled/verified and registered seams.

## Verification Evidence

- **Focused Power Applet tests (4/4 passed)**:
  `ctest --test-dir build/dev -R '^qindaqt\.power-applet-' --output-on-failure --no-tests=error`
  - `qindaqt.power-applet-presentation` -> PASSED
  - `qindaqt.power-applet-control-rows` -> PASSED
  - `qindaqt.power-applet-request-state` -> PASSED
  - `qindaqt.power-applet-boundary` -> PASSED
- **Adjacent Power and Brightness tests (10/10 passed)**:
  `ctest --test-dir build/dev -R '(power|brightness)' --output-on-failure --no-tests=error`
  - `qindaqt.power-protocol-values` -> PASSED
  - `qindaqt.power-protocol-codec` -> PASSED
  - `qindaqt.power-aggregation-model` -> PASSED
  - `qindaqt.brightness-model-math` -> PASSED
  - `qindaqt.brightness-model-composition` -> PASSED
  - `qindaqt.brightness-model-boundary` -> PASSED
  - `qindaqt.power-applet-presentation` -> PASSED
  - `qindaqt.power-applet-control-rows` -> PASSED
  - `qindaqt.power-applet-request-state` -> PASSED
  - `qindaqt.power-applet-boundary` -> PASSED
- **Static Boundary Gate**:
  `cmake -DSOURCE_ROOT=. -P tests/shell/power_applet/check_boundary.cmake` -> PASSED (10 files scanned)
- **Source Shape Gate**:
  `tools/check-source-shape` -> PASSED (1024 source files checked, 0 warnings)
- **Documentation Validation Gate**:
  `tools/validate-docs` -> PASSED (66 Markdown documents and mkdocs.yml navigation)
- **Whitespace / Git Diff Check**:
  `git diff --check` -> PASSED (clean)

## Requested Next Action

Requesting an independent Claude or GLM reviewer to conduct an exact-commit review of `d11a69d36c30d5100c3878fd0fa505c792ad1c6b` on branch `worker/power-applet-p1-repair-sela`.
