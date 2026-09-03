# Dana Ulery exact-candidate recheck

- Persona: Dana Ulery
- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Candidate SHA: `1d86b1349cc51c0516c269e1f9eb228fd6fb20cc`
- Tree SHA: `669f60fa8861ec646ed0613388a149dcba110f84`
- Parent SHA: `fbf0de92d0a8b4f4e68b0286032bcc291dfa092a`
- Base SHA: `34db07da093a461c22a3779e0f5e7192d40f6e59`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/panel-visibility-kwin666-codex-review`

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

1. The repository-wide source-shape gate still exits 1 on the inherited, untouched `src/apps/file_manager/main.cpp:178`: `main` spans 181 lines against the 180-line function limit. Reproduction: `./tools/check-source-shape`. Observed: that single error plus nonblocking decomposition warnings; expected: exit 0. The candidate diff does not touch File Manager. The owned `./tools/check-source-shape --root tests/session --largest 20` gate exits 0 across 149 files.

## Recheck results

- Orderly exit is fixed. Across the 12 fresh Debug/Release archives, exact scans found zero `Session process has crashed` lines. Every evidence document reports bounded cleanup, an empty survivor set, and exactly one authenticated `private-bus`; the host process check was empty after every invocation.
- The Release race did not recur and no retry was used. Both independent Release invocations passed boot, 1080p panel visibility, and WUXGA panel visibility on their first attempts. Exact scans found zero `multiple processes claimed private-bus`, portal-activation, or `xdg-desktop-portal` lines. PipeWire's expected `Portal not found` absence warning is not an activation.
- The new compiled loader row is non-vacuous. A scratch copy below the assigned build root with only `process.setProcessEnvironment(environment)` omitted configured and built, then CTest exited 8: `appliesLoaderPathOnlyToConfiguredChild` failed because the applied `WAYLAND_DISPLAY` was empty (3 QtTest assertions passed, 1 failed). The unmodified row passes in Debug and Release.
- Validator strictness remains intact. The verbose Debug validator run passed 3 Python hostile tests and 5 QtTest assertions, including never-unmapped zero-size and escaped-geometry rejection. Every fresh panel archive contains exactly 8 phases and 8 captures.

## Commands and results

- Identity/cleanliness: `git rev-parse HEAD HEAD^{tree} HEAD^`, `git merge-base 34db07d 1d86b1349cc51c0516c269e1f9eb228fd6fb20cc`, and `git status --porcelain` — hashes above, base `34db07da093a461c22a3779e0f5e7192d40f6e59`, status empty before and after.
- KWin: `/usr/bin/kwin_wayland --version` — exit 0, `kwin 6.6.6`.
- Configure, run once for `Debug/debug` and once for `Release/release`:

  ```sh
  cmake -S . -B <ROOT>/<profile> -G Ninja \
    -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake \
    -DCMAKE_BUILD_TYPE=<type> -DBUILD_TESTING=ON \
    -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
    -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
    -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
    -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
  ```

  Both exit 0. The lane-specific system-KWin cache was required by the review lane.

- Focused build, run in both profiles:

  ```sh
  cmake --build <ROOT>/<profile> --parallel 3 --target \
    qindaqt-panel-visibility-capture-loader-tests \
    qindaqt-panel-visibility-phase-settlement-tests \
    qindaqt-panel-visibility-session-probe qindaqt-desktop-session-probe \
    qindaqt_panel_visibility_modes_tests qindaqt_panel_visibility_scope_tests \
    qindaqt_panel_visibility_validation_tests \
    qindaqt_compositor_visibility_snapshot_tests \
    qindaqt_compositor_visibility_state_tests \
    qindaqt_compositor_visibility_wire_roundtrip_tests \
    qindaqt_compositor_visibility_client_tests \
    qindaqt_qt_compositor_visibility_transport_tests \
    qindaqt_panel_visibility_inventory_assembler_tests \
    qindaqt_panel_visibility_producer_tests \
    qindaqt_panel_visibility_popup_bounds_tests \
    qindaqt_panel_visibility_settings_private_bus_tests \
    qindaqt_shell_runtime_options_tests \
    qindaqt_shell_visibility_refresh_scheduler_tests \
    qindaqt_shell_visibility_snapshot_tests \
    qindaqt_shell_visibility_window_admission_tests \
    qindaqt-shell qindaqt-shell-preview
  ```

  Debug and Release exit 0; each incremental build completed 36/36 executed Ninja actions.

- Focused unit selector, run in both profiles:

  ```sh
  ctest --test-dir <ROOT>/<profile> \
    -R '^(qindaqt\.shell-(visibility|orchestration-visibility|runtime-)|desktop\.virtual\.panel-visibility\.(capture-loader(-cpp)?-unit|validator-unit))' \
    --output-on-failure --no-tests=error
  ```

  Debug 23/23 and Release 23/23, both exit 0.

- `ctest --test-dir <ROOT>/<profile> -R '^desktop\.virtual\.sandbox-unit$' --output-on-failure --no-tests=error` — Debug 1/1 and Release 1/1, both exit 0. A verbose Debug replay reports 121/121 Python tests.
- Live command, run twice in Debug and twice in Release after an empty `pgrep -f '[k]win_wayland'`, with `--parallel 1`:

  ```sh
  QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
  ctest --test-dir <ROOT>/<profile> --parallel 1 \
    -R '^(desktop\.virtual\.boot\.1080p|desktop\.virtual\.panel-visibility\.single-(1080p|wuxga))$' \
    --output-on-failure --no-tests=error
  ```

  All four invocations exit 0 and report 4/4 including the automatically required package fixture. Debug totals: 93.50 s and 92.47 s. Release totals: 92.41 s and 92.86 s. Thus the three requested live rows pass twice per profile, 12/12 requested executions total.
- Fresh archive audit: `python3 -m json.tool` on all 12 `result.json` and all 12 evidence JSON files — exit 0. All results are success/0/not-timed-out; eight panel archives have 8 phases and 8 captures; all 12 have bounded cleanup, empty survivors, and one private bus. Exact `rg` scans for the crash, duplicate-bus, and portal-activation diagnostics return no matches.
- Static gates: `./tools/validate-docs` exit 0 (138 documents); `mkdocs build --strict --site-dir <ROOT>/site` exit 0; `git diff --check 34db07d..1d86b13` and `git diff --check` exit 0; no JSON changed. Repository-wide `./tools/check-source-shape` exits 1 only as P3-1; owned session shape exits 0.

## Verdict

ACCEPT. The three prior blocking findings are repaired on the exact candidate: teardown is orderly and strictly diagnosed, Release passes are unretried and repeatable, and the C++ loader application has a mutation-sensitive compiled row.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/1
