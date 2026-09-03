# Adele Goldstine — independent exact-candidate repair recheck

- Reviewer: **Adele Goldstine** (`adele-goldstine`), independent first-party route reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `e537fc9ad4c6bcf118cc1ec8646cf3211cd96e74`
- Candidate tree SHA: `2f8e4b8cb2f6504a56c17884c84c1614b72e555c`
- Parent SHA: `c09980f6b1c496ba4ddb7ff92370e1b6178bc3a2`
- Base SHA: `03dc71e6dfd06b6e30f8f86ac354fe24684f497a`
- Repaired product ancestor: `5a411b52da08f09a58be351daa47b60b1ef51a6c`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas-k3-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex`
- Scratch root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch`

The worktree was detached at the exact candidate and `git status --porcelain`
was empty before and after review. Scratch probes remained under the assigned
build root. No product path was edited, committed, amended, or rebased.

## Verdict summary

**REJECT** — the responsive close-state defect is closed in both directions,
and the repaired exact private-header poison is now rejected. One P2 remains:
the checker and its sole negative control still do not enforce the documented
ban on private repository headers generally. An existing sibling-module private
header included by its direct repository path passes the checker.

## Findings ledger

### P0

None.

### P1

None.

### P2

#### P2-1 — the boundary checker accepts a private sibling-module repository header

- Contract and test locations:
  - `docs/wiki/architecture/module-boundaries.md:87` permits only named public
    dependencies and says Customize must never import private repository headers.
  - `docs/wiki/apps/customize-settings.md:107-110` claims the positive row
    rejects private-repository imports.
  - `tests/apps/settings/customize/check_boundary.cmake:17-24,34-39` recognizes
    only three named module paths, `_p.h`, and nested `src/**/src/` shapes. It
    does not reject a direct private path such as `src/apps/settings_center/*.h`.
  - The registered control at
    `tests/apps/settings/customize/check_boundary_negative.cmake:11-12` plants
    only the exact `shell_customization/src/..._p.h` form already named by the
    matcher, so it cannot prove the broader contract.
- Scratch reproduction input:
  `/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/boundary-own-hostile/private_settings_center_header.cpp:2`
  contains:

  ```cpp
  #include "src/apps/settings_center/settings_route_registry.h"
  ```

- Exact reproduction:

  ```sh
  cmake \
    -DSCAN_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/boundary-own-hostile \
    -P tests/apps/settings/customize/check_boundary.cmake
  ```

  Observed: exit 0 with no diagnostic. Expected: nonzero rejection because
  `settings_route_registry.h` is an internal header reached through its
  repository `src/` path, not a public API of any allowed Customize dependency.
  The real route passes the positive scan and does not currently include this
  header, so this is a missing negative control/documentation overclaim rather
  than a live dependency crossing. It is P2 under the brief's explicit
  missing-negative-control rule.

- Controls:

  ```sh
  cmake \
    -DPOISON_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/registered-private-poison \
    -DCHECK_SCRIPT=/home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas-k3-review/tests/apps/settings/customize/check_boundary.cmake \
    -P tests/apps/settings/customize/check_boundary_negative.cmake
  ```

  Exit 0: the wrapper observed rejection of its exact poison.

  ```sh
  cmake \
    -DSCAN_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/boundary-private-poison \
    -P tests/apps/settings/customize/check_boundary.cmake
  ```

  Exit 1 with `Customize editor imported a private repository header`, proving
  the rejected ancestor's exact `layout_editing_repository_p.h` form is fixed.

### P3

None.

## Repair-question answers

1. **Prior responsive-close P2 is closed.** The external probe passed
   wide-to-compact and compact-to-wide in Debug and Release. Each run observed
   `closeAccepted=false`, `dialogAfterResize=true`,
   `applicationClosePending=true`, `windowVisible=true`, and `dirty=true`.
   The registered lifecycle test explicitly exercises both directions and the
   warning-fatal row passes in both configurations.
2. **Prior exact private-header form is closed, but the boundary requirement is
   not fully closed.** The exact registered poison is rejected; the independent
   private sibling-header form above is accepted.
3. **Nothing moved outside the bounded repair scope.** The descendant changes
   only the eight declared documentation, QML, lifecycle-test, and boundary-test
   paths. No registry/build file changed relative to `5a411b5`. Relative to the
   base, Customize remains appended after Network; target links/dependencies and
   the install RPATH are additive, with no existing Settings route removed or
   reordered.
4. **Required evidence rerun.** Customize is 6/6, warning-fatal page/lifecycle
   is 2/2, the customization/domain selector is 17/17, and Settings Center is
   9/9 in both Debug and Release. All required static gates pass.

## Exact commands and results

### Identity and cleanliness

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git status --porcelain
```

Observed candidate/tree/parent exactly as recorded above and empty status both
before and after review. `git merge-base --is-ancestor <base> <candidate>`
exited 0.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both exited 0. CMake emitted the known dependency-prefix runtime-path warnings
outside candidate scope.

### Focused build

For each of `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/<profile> \
  --parallel 3 --target \
  qindaqt_settings_customize_model_tests \
  qindaqt_settings_customize_page_tests \
  qindaqt_settings_customize_window_lifecycle_tests \
  qindaqt-settings \
  qindaqt_settings_route_registry_test \
  qindaqt_settings_navigation_controller_test \
  qindaqt_settings_navigation_page_test \
  qindaqt_panel_editing_tests qindaqt_applet_editing_tests \
  qindaqt_preview_history_tests qindaqt_coordinator_lease_tests \
  qindaqt_editor_query_tests qindaqt_customize_editor_intent_tests \
  qindaqt_customize_editor_gesture_tests \
  qindaqt_customize_editor_session_tests \
  qindaqt_customize_editor_dirty_state_tests \
  qindaqt_customize_editor_persistence_tests \
  qindaqt_customize_editor_accessibility_tests
```

Both exited 0; each incremental build completed 21/21 executed Ninja steps.

### Responsive-close external probe

The scratch probe was rebuilt for each profile from the lifecycle target's
emitted compile/link commands (`ninja -C <profile> -t commands
qindaqt_settings_customize_window_lifecycle_tests`) and run as follows:

```sh
env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/responsive_close_probe

env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/responsive_close_probe \
  --compact-to-wide

env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/responsive_close_probe_release

env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/responsive_close_probe_release \
  --compact-to-wide
```

All four exited 0 with the shared pending/dialog/window/dirty truth described
above.

### Tests

For each profile:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/<profile> \
  -R '^qindaqt\.settings-customize-' --output-on-failure --no-tests=error
```

Debug and Release: exit 0, 6/6 passed.

```sh
env QT_FATAL_WARNINGS=1 ctest \
  --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/<profile> \
  -R '^qindaqt\.settings-customize-(page|window-lifecycle)$' \
  --output-on-failure --no-tests=error
```

Debug and Release: exit 0, 2/2 passed.

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/<profile> \
  -R 'customiz' --output-on-failure --no-tests=error
```

Debug and Release: exit 0, 17/17 passed.

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/<profile> \
  -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' \
  --output-on-failure --no-tests=error
```

Debug and Release: exit 0, 9/9 passed.

One superseded diagnostic invocation applied `QT_FATAL_WARNINGS=1` to the
entire six-row Customize selector. It exited 8 with 5/6 passing because the
installed-package row deliberately starts against an absent private bus and
its expected warning became fatal (return 1 instead of the harness's expected
root-construction return 3). The brief requires fatal warnings on the page and
lifecycle rows, not the absent-bus package row; the two exact commands above
were then run separately and both passed in each profile.

### Static gates

```sh
./tools/validate-docs
```

Exit 0: 117 Markdown documents and `mkdocs.yml` navigation validated.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/site
```

Exit 0.

```sh
./tools/check-source-shape
```

Exit 0: 1,787 files checked; only the two pre-existing decomposition-review
warnings outside candidate paths were reported.

```sh
git diff --check
git diff --check 03dc71e6dfd06b6e30f8f86ac354fe24684f497a \
  e537fc9ad4c6bcf118cc1ec8646cf3211cd96e74
```

Both exited 0. No JSON file changed from base to candidate, so no
`python3 -m json.tool` invocation applies.

## Unavailable or prohibited coverage

No mandated gate was unavailable. Per the lane prohibition, no `tests/session`
nested-compositor row, host D-Bus service, hardware, uinput, or network was run.

## Verdict

The close-state repair is sound, but ACCEPT requires zero P2 findings. The
source-policy checker still accepts an ordinary private repository header and
its documentation claims more than the registered negative control proves.

VERDICT REJECT P0/P1/P2/P3=0/0/1/0
