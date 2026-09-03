# Lynn Conway — second Customize canvas repair handoff

- Timestamp: `2026-09-02T23:56:38-06:00`
- Exact candidate commit: `e537fc9ad4c6bcf118cc1ec8646cf3211cd96e74`
- Candidate tree: `2f8e4b8cb2f6504a56c17884c84c1614b72e555c`
- Candidate parent: `c09980f6b1c496ba4ddb7ff92370e1b6178bc3a2`
- Exact rejected product ancestor: `5a411b52da08f09a58be351daa47b60b1ef51a6c`
- Exact base: `03dc71e6dfd06b6e30f8f86ac354fe24684f497a`
- Branch: `worker/customize-settings-canvas`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas`

## Outcome

The Settings window now owns one application-close-pending value and passes it
to both responsive route hosts. A dirty close remains unresolved across
wide-to-compact and compact-to-wide reconstruction; the active host recreates
the modal until Cancel or Discard clears the shared decision. The lifecycle
test locates the dialog in the newly active host and verifies window, dirty,
and pending-state truth before and after Cancel.

The Customize boundary checker now rejects private source paths for
`shell_customization`, `shell_customization_editor`, and `profiles`, any
`_p.h` include, and general `src/**/src/` includes. Its hostile control plants
exactly:

```cpp
#include "src/shell_customization/src/layout_editing_repository_p.h"
```

and requires the checker to reject it. The owning documentation now states the
same exact proof rather than claiming multiple synthesized poisons.

## Changed product paths

- `docs/wiki/apps/customize-settings.md`
- `docs/wiki/apps/settings-center.md`
- `docs/wiki/development/testing-harness.md`
- `src/apps/settings_center/Main.qml`
- `src/apps/settings_center/SettingsRouteHost.qml`
- `tests/apps/settings/customize/check_boundary.cmake`
- `tests/apps/settings/customize/check_boundary_negative.cmake`
- `tests/apps/settings/customize/tst_customize_window_lifecycle.cpp`

## Verification evidence

Red controls against the unrepaired tree:

```sh
env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/responsive_close_probe
```

Exit 1 as expected: `dialogAfterResize=false` with the window visible and draft
dirty.

```sh
cmake \
  -DSCAN_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/boundary-private-poison \
  -P tests/apps/settings/customize/check_boundary.cmake
```

Exit 0 on the unrepaired checker as expected, proving the negative control was
non-vacuous. The repaired checker returns exit 1 for that same poison.

Both strict configurations were regenerated with the lane's exact recipe:

```sh
cmake -S . -B <ROOT>/<profile> -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=<Debug-or-Release> -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Debug and Release both exited 0. CMake emitted only the known dependency-prefix
runtime-path warnings outside this candidate.

For each of Debug and Release:

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_settings_customize_model_tests \
  qindaqt_settings_customize_page_tests \
  qindaqt_settings_customize_window_lifecycle_tests \
  qindaqt-settings qindaqt_settings_route_registry_test \
  qindaqt_settings_navigation_controller_test \
  qindaqt_settings_navigation_page_test qindaqt_panel_editing_tests \
  qindaqt_applet_editing_tests qindaqt_preview_history_tests \
  qindaqt_coordinator_lease_tests qindaqt_editor_query_tests \
  qindaqt_customize_editor_intent_tests \
  qindaqt_customize_editor_gesture_tests \
  qindaqt_customize_editor_session_tests \
  qindaqt_customize_editor_dirty_state_tests \
  qindaqt_customize_editor_persistence_tests \
  qindaqt_customize_editor_accessibility_tests
```

Both exited 0 with 21/21 incremental Ninja steps. After the final state
assertions, the lifecycle target rebuilt 3/3 steps in each configuration.

```sh
ctest --test-dir <ROOT>/<profile> \
  -R '^qindaqt\.settings-customize-' --output-on-failure --no-tests=error
```

Debug: exit 0, 6/6 passed. Release: exit 0, 6/6 passed. The page and lifecycle
rows carry `QT_FATAL_WARNINGS=1` in their registered environments.

```sh
ctest --test-dir <ROOT>/<profile> \
  -R 'customiz' --output-on-failure --no-tests=error
```

Debug: exit 0, 17/17 passed. Release: exit 0, 17/17 passed.

```sh
ctest --test-dir <ROOT>/<profile> \
  -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' \
  --output-on-failure --no-tests=error
```

Debug: exit 0, 9/9 passed. Release: exit 0, 9/9 passed.

The final lifecycle-only rerun after adding explicit shared-state assertions
passed 1/1 in both Debug and Release. The boundary/lifecycle subset passed 3/3
in both configurations. Direct repaired boundary checks returned: real route
exit 0, registered private poison exit 0 because rejection was observed, and
the same poison passed straight to the checker exit 1.

Static gates:

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/site
./tools/check-source-shape
git diff --check
```

All exited 0. Documentation validated 117 Markdown documents plus navigation.
Source shape checked 1,787 files and reported only the two pre-existing
decomposition-review warnings outside candidate paths. No JSON changed, so no
JSON parser gate applies.

## Bounded caveats

- This is offscreen, fake-transport, temporary-store, source-policy, and
  relocated-package evidence only.
- Per lane prohibition, no nested compositor/session row, host D-Bus service,
  hardware, uinput, or network was contacted.
- Live shell binding, always-hidden reveal behavior, live AT-SPI, installed
  session behavior, and the rendered desktop matrix remain later slices.

## Requested next action

Please route this exact candidate back to **Adele Goldstine (OpenAI Codex)** for
an independent exact recheck, then manager integration if accepted.
