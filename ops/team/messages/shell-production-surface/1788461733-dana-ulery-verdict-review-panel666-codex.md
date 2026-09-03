# Dana Ulery exact-candidate review

- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Candidate: `34db07da093a461c22a3779e0f5e7192d40f6e59`
- Tree: `9574cf42d05b8d4afd3e44ec4ce2b24652b66aa1`
- Parent: `aa862093993cc263c0059efca0f67282fcdb265a`
- Base: `aa862093993cc263c0059efca0f67282fcdb265a`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/panel-visibility-kwin666-codex-review`

## Findings ledger

### P0

None.

### P1

1. **KWin still reports a crashed session at teardown.** Candidate documentation explicitly permits this at `docs/wiki/shell/panel-visibility.md:189`, while cleanup is initiated at `tests/session/test_panel_visibility_nested.py:314`. Reproduction: run the three required nested selectors twice per configuration with `QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop`, `--parallel 1`, then run `rg -n 'Session process has crashed' <ROOT>/{debug,release}/tests/session/desktop-session-results/*/logs/compositor.log`. Observed: five hits—Debug runs `41e2b5f41ce6a655027a8f4091804e86` and `997927d4704655586156a52ff49cb5ef`; Release runs `0a30f2284324727d78c1760f56178f6c`, `8e37c2263a912b8a003021168448ed45`, and `ed71abe7b1359043ef1915f04f85582b`. Expected: no such diagnostic under the lane's explicit teardown criterion. Every invocation still left zero `kwin_wayland` survivors, so this is the diagnostic itself, not a survivor inference.

2. **The funded two-pass Release qualification is not reliable.** Reproduction: `QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop ctest --test-dir <ROOT>/release -R '^desktop\.virtual\.panel-visibility\.single-1080p$' --output-on-failure --no-tests=error --parallel 1`. The first repetition failed after 3.27 s with `panel visibility qualification failed: multiple processes claimed private-bus`, raised by `tests/session/desktop_session_process.py:185`; the second passed. Expected: both repetitions pass. No KWin process survived the failed row.

### P2

1. **The added unit row does not prove the C++ half of the loader repair fails without the fix.** `tests/session/PanelVisibilityTests.cmake:31` registers only the Python test, which checks command construction at `tests/session/test_panel_visibility_capture_loader_unit.py:21`; it never executes the loader injection at `tests/session/panelvisibilitysessionprobe.cpp:172`. Reproduction: `ctest --test-dir <ROOT>/debug -N -V -R '^desktop\.virtual\.panel-visibility\.capture-loader-unit$'` lists a single direct Python command and no probe executable or fixture. Observed: this row remains independent of the production test probe code that sets the child environment. Expected: the claimed unit negative control must fail if that application step is absent.

### P3

1. `./tools/check-source-shape` exits 1 on the immutable candidate because `src/apps/file_manager/main.cpp:178` spans 181 lines (limit 180). The seven-file candidate diff does not touch that path. Running the same gate from a `git archive main` scratch tree exits 0 (2,416 files), confirming the implementer's blocker note is stale and the integration branch already carries the unrelated repair.

## Mechanism and strictness checks

- The preserved failing run is consistent with a mixed-ABI capture failure after valid panel authority. Directly running the private `weston-screenshooter` with `LD_LIBRARY_PATH` unset exited 127 (`libweston-15.so.0` missing); supplying `/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-arch-665/root/usr/lib:/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-arch-665/root/usr/lib/weston` cleared the loader error and reached the deliberately absent Wayland socket (exit 255). Candidate code validates that path at `tests/session/panel_visibility_capture.py:18` and applies it only to the screenshot child at `tests/session/panelvisibilitysessionprobe.cpp:179`.
- The registered capture-loader unit passed in Debug and Release. Running it against an exact-base source archive failed at import because the helper module does not exist there; that is only coarse before/after evidence, not the missing C++ negative control above.
- A hostile in-memory interaction with a positive-size left surface in `window-overlap-hidden` was rejected with `left panel is not hidden in window-overlap-hidden`, exercising `tests/session/test_panel_visibility_nested.py:166`. The bottom-panel hidden check remains at line 180.

## Commands and results

- Identity/cleanliness: `git rev-parse HEAD HEAD^{tree} HEAD^`, `git status --porcelain` — exact hashes above; empty before and after review.
- Configure Debug and Release: `cmake -S . -B <ROOT>/<debug|release> -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=<Debug|Release> -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON` — both exit 0.
- Focused builds: `cmake --build <ROOT>/<debug|release> --parallel 3 --target qindaqt-panel-visibility-phase-settlement-tests qindaqt-panel-visibility-session-probe qindaqt-desktop-session-probe qindaqt_panel_visibility_modes_tests qindaqt_panel_visibility_scope_tests qindaqt_panel_visibility_validation_tests qindaqt_compositor_visibility_snapshot_tests qindaqt_compositor_visibility_state_tests qindaqt_compositor_visibility_wire_roundtrip_tests qindaqt_compositor_visibility_client_tests qindaqt_panel_visibility_inventory_assembler_tests qindaqt_panel_visibility_producer_tests qindaqt_panel_visibility_popup_bounds_tests qindaqt_panel_visibility_settings_private_bus_tests qindaqt_shell_runtime_options_tests qindaqt_shell_visibility_refresh_scheduler_tests qindaqt_shell_visibility_snapshot_tests qindaqt_shell_visibility_window_admission_tests qindaqt-shell qindaqt-shell-preview` — both exit 0 (1,516/1,516 steps). The first test pass exposed one omitted adjacent target; `cmake --build <ROOT>/<debug|release> --parallel 3 --target qindaqt_qt_compositor_visibility_transport_tests` then passed 4/4 in each configuration.
- Unit selectors: `ctest --test-dir <ROOT>/<debug|release> -R '^(qindaqt\.shell-(visibility|orchestration-visibility|runtime-)|desktop\.virtual\.panel-visibility\.(capture-loader-unit|validator-unit))' --output-on-failure --no-tests=error` — final reruns passed 22/22 in Debug and 22/22 in Release. Initial pre-adjacent-target runs were 21/22 with one Not Run (exit 8).
- Nested selectors: each of `desktop.virtual.boot.1080p`, `desktop.virtual.panel-visibility.single-1080p`, and `desktop.virtual.panel-visibility.single-wuxga` was run twice per configuration, serially, with the private lane and an empty pre-run `pgrep -f kwin_wayland`. Debug: 6/6 selector invocations passed. Release: 5/6 passed; the first 1080p visibility invocation failed as P1-2. All post-run survivor checks were empty. Successful visibility evidence contained exactly eight phases and eight captures.
- Static gates: `./tools/validate-docs` exit 0 (138 documents); `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` exit 0; `git diff --check aa86209..34db07d` and `git diff --check` exit 0; no JSON changed. `./tools/check-source-shape` exit 1 as P3-1; current `main` scratch archive exit 0.

## Verdict

REJECT. The named loader mechanism works and the validator remains strict, but the exact candidate fails the required teardown diagnostic and repeatability criteria, and its unit row does not isolate the C++ environment-application step.

VERDICT REJECT P0/P1/P2/P3=0/2/1/1
