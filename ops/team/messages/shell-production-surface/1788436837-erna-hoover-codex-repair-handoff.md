# Erna Hoover-Codex — repaired panel visibility candidate handoff

- Timestamp: 2026-09-03T06:00:37-06:00
- Exact candidate commit: `e64418798cdc2ef7e4244f07664f8c9fda98a862`
- Candidate tree: `bfde197b3d9a41f5536b2324bf30f947e284020c`
- Rejected candidate repaired: `a52561f6accaa2cd43e20ba7251e446d8c5ae1ad`
- Exact lane base: `349f805b685c0b5b1ad146d600dc1c3fa528281d`
- Branch: `worker/panel-visibility-proof`
- Requested next action: independent exact review by Dana Ulery, then manager integration.

## Finding closure

- **P1-1** — closed by `e64418798cdc2ef7e4244f07664f8c9fda98a862` and
  `qindaqt.shell-visibility-settings-private-bus`. The runtime now accepts only
  the Settings1 canonical signed-64-bit integer, and a real private D-Bus
  Settings1 round trip proves `qint64(400)` plus `reducedMotion=false` selects
  the uncapped 320 ms animation.
- **P1-2** — closed by the same commit and
  `qindaqt.shell-visibility-popup-bounds`. Admissions are capped at 32 sources,
  128 aggregate leases, and 128 UTF-16 source-id units. Each admission is
  fenced to one `QObject`, owner loss/destruction releases it, topology and
  producer teardown release it, duplicate visible signals cannot renew it,
  and a 30,000 ms hard timer expires an uninterrupted hold.
- **P1-3** — closed by the same commit,
  `desktop.virtual.panel-visibility.validator-unit`, and both installed rows.
  Eight captures are joined to each phase's compositor-authority panel
  rectangle, the complete panel rectangle is sampled, hidden/visible region
  digests must differ, and eight identical unrelated images are rejected.
- **P1-4** — closed by the same validator and installed rows. Evidence persists
  the final hidden pre-drag surface snapshot, authoritative before/after window
  frames, rail clearance, a second hidden capture immediately before close,
  an intersecting pre-close frame, post-close compositor absence, and a fresh
  restored-panel capture. Final runs moved 1080p `(0,30,722x517)` to
  `(0,336,722x517)` and WUXGA `(0,30,722x517)` to `(0,402,722x517)`.

The reviewer reproductions were executed before repair exactly as recorded in
the rejection verdict. Each exited `1` after detecting the rejected behavior:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/capture_phase_vacuity.py
python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/move_close_causality.py
QT_QPA_PLATFORM=offscreen /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/producer_contract_repro
```

## Changed paths

```text
docs/wiki/development/testing-harness.md
docs/wiki/shell/panel-visibility.md
src/shell/runtime/panelvisibilitypopup.cpp
src/shell/runtime/panelvisibilitypopup.h
src/shell/runtime/panelvisibilityruntime.cpp
tests/session/CMakeLists.txt
tests/session/PanelVisibilityTests.cmake
tests/session/fixtures/panel_visibility_profiles/panel-visibility-proof.json
tests/session/panelvisibilitysessionprobe.cpp
tests/session/panelvisibilitysessionwindowproof.cpp
tests/session/panelvisibilitysessionwindowproof.h
tests/session/test_desktop_session_panel_visibility_unit.py
tests/session/test_panel_visibility_nested.py
tests/shell_visibility_producers/CMakeLists.txt
tests/shell_visibility_producers/tst_panelvisibilitypopupbounds.cpp
tests/shell_visibility_producers/tst_panelvisibilityproducers.cpp
tests/shell_visibility_producers/tst_panelvisibilitysettingsprivatebus.cpp
```

## Verification evidence

Both prescribed configurations completed with exit `0`:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/panel-visibility-proof/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/panel-visibility-proof/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Focused strict-warning builds completed with exit `0` in Debug and Release:

```sh
cmake --build <config-root> --parallel 3 --target \
  qindaqt_shell_panel_visibility_producers qindaqt-shell \
  qindaqt_panel_visibility_producer_tests \
  qindaqt_panel_visibility_popup_bounds_tests \
  qindaqt_panel_visibility_settings_private_bus_tests \
  qindaqt_panel_visibility_modes_tests qindaqt_panel_visibility_scope_tests \
  qindaqt_panel_visibility_validation_tests \
  qindaqt_compositor_visibility_snapshot_tests \
  qindaqt_compositor_visibility_state_tests \
  qindaqt_compositor_visibility_wire_roundtrip_tests \
  qindaqt_compositor_visibility_client_tests \
  qindaqt_qt_compositor_visibility_transport_tests \
  qindaqt_panel_interaction_store_tests \
  qindaqt_panel_visibility_inventory_assembler_tests \
  qindaqt_panel_runtime_plan_assembler_tests qindaqt_output_inventory_matcher_tests \
  qindaqt_shell_runtime_options_tests qindaqt_shell_visibility_snapshot_tests \
  qindaqt_shell_visibility_window_admission_tests \
  qindaqt_shell_visibility_refresh_scheduler_tests qindaqt-shell-preview \
  qindaqt_shell_launcher_qmlplugin qindaqt_controls_qmlplugin
cmake --build <config-root> --parallel 3 --target qindaqt-panel-visibility-session-probe
```

The final selector command passed `27/27` in Debug and `27/27` in Release,
including eight producer rows, the private-bus and hostile popup tests, the
validator unit, all visibility policy/client/transport/orchestration rows, and
all three shell-runtime rows:

```sh
ctest --test-dir <config-root> \
  -R '^(compositor\.shell-visibility|qindaqt\.shell-visibility|qindaqt\.shell-orchestration-(interactions|visibility-inventory|runtime-plan|output-match)|qindaqt\.shell-runtime|desktop\.virtual\.panel-visibility\.validator-unit)' \
  --output-on-failure --no-tests=error
```

The required prerequisite passed `1/1`:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/panel-visibility-proof/debug \
  -R '^desktop\.virtual\.sandbox-unit$' --output-on-failure --no-tests=error
```

The final-tree nested matrix ran twice, serially. Run 1 passed `3/3` in 90.10
seconds; run 2 passed `3/3` in 89.30 seconds:

```sh
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/panel-visibility-proof/debug \
  --parallel 1 \
  -R '^desktop\.virtual\.panel-visibility\.(single-1080p|single-wuxga)$' \
  --output-on-failure --no-tests=error
```

The four final archives are `7222d4297d59b7d3ab785339121e61ef`,
`e55a23e1495699095005db9fdea13528`,
`3c03074abc826dbe5df8418a0efe65f6`, and
`ff4e912a3f46e599b92bc6379a55f787`. Each reports eight captures,
`hostDisplayReachable=false`, `hostInputReachable=false`,
`hostSessionBusReachable=false`, and `survivorPids=[]`. Process audits before,
between, and after runs found no candidate KWin, Weston, probe, or nested driver.

Final static gates all exited `0`:

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/panel-visibility-proof/site
./tools/check-source-shape
git diff --check
python3 -m json.tool tests/session/fixtures/panel_visibility_profiles/panel-visibility-proof.json > /dev/null
```

`validate-docs` checked 127 documents/navigation entries. Source shape checked
2064 files; its only warnings are three pre-existing files outside this lane
(`tests/compositor/CMakeLists.txt`, display-color model tests, and audio-applet
tests). No changed source reaches the 500-line review threshold.

## Bounded caveats

This candidate deliberately claims only contained, software-rendered 100% 1080p
and WUXGA behavior. It does not claim fractional scaling, multiple outputs,
GPU/OpenGL rendering, physical input, host desktop integration, pixel-perfect
aesthetic baselines, or other profiles/themes. The 30-second popup limit is a
hard uninterrupted-admission lifetime; an owner must close and reopen to
obtain a new hold. No hardware, host bus, host display, uinput, or network test
was run.
