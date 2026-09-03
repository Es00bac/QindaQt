# Adele Goldstine — independent exact-candidate repair recheck

- Reviewer: **Adele Goldstine** (`adele-goldstine`), independent first-party route reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `5a411b52da08f09a58be351daa47b60b1ef51a6c`
- Candidate tree SHA: `7ac888684b5b4f302073a48060e019cb753f1bef`
- Parent SHA: `80752f96bba76ff7f2c277ea8c2ffa3611f0b2ff`
- Base SHA: `03dc71e6dfd06b6e30f8f86ac354fe24684f497a`
- Rejected product ancestor: `a6ea864ec2162dd5c1a8fba9f2d8f1f2f09a321f`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas-k3-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex`
- Scratch root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch`

The worktree was detached at the exact candidate and `git status --porcelain`
was empty before and after review. Scratch probes were kept outside the
worktree. No product path was edited, committed, amended, or rebased.

## Verdict summary

**REJECT** — the two P2 defects from the prior verdict are repaired in their
original forms, and all required builds/selectors/static gates pass. Two new
P2 findings remain: the repaired top-level-close state is scoped to one
responsive host and loses its modal on a host switch, and the advertised
private-repository boundary negative control is missing.

## Findings ledger

### P0

None.

### P1

None.

The dialog constructs without QML reference errors, is centered, and survives
fatal warnings. Dirty ordinary-close and Ctrl+Q requests initially open the
confirmation, and Cancel keeps the window and draft. No host bus, compositor,
hardware, input device, or network was contacted.

### P2

#### P2-1 — A pending dirty top-level close loses its modal when the responsive host changes

- Product locations:
  - `src/apps/settings_center/Main.qml:34-38` sends close state only to the host
    active at the instant of the close event.
  - `src/apps/settings_center/SettingsRouteHost.qml:21,64-72` stores
    `applicationClosePending` separately in each host.
  - `src/apps/settings_center/SettingsRouteHost.qml:121-136` reconstructs the
    dialog only when that same host's local pending flag is true.
  - `docs/wiki/development/testing-harness.md:802-804` claims the lifecycle row
    preserves the pending prompt across wide/compact host reconstruction.
- Cause: a dirty close in wide mode sets only `wideRouteHost.applicationClosePending`.
  Resizing below 540 makes that host presentation-inactive and destroys its
  `CustomizeRoute`. The compact host retains the shared dirty model but its
  local pending flag is false, so it creates Customize without reopening the
  dialog. The window is usable while a close decision remains stranded in the
  inactive host; resizing back or issuing another close is the bounded
  workaround.
- Exact reproduction, using the candidate's QML and test stubs from an
  external scratch binary built with the focused lifecycle target's exact
  compile/link flags:

  ```sh
  env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 QT_FATAL_WARNINGS=1 \
    /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/responsive_close_probe
  ```

  Debug observed, exit 1:

  ```text
  observed closeAccepted= false dialogAfterResize= false windowVisible= true dirty= true
  expected pending top-level close dialog to reconstruct in compact host
  ```

  The identically built Release probe produced the same output and exit 1.
  Expected: after the close was rejected, the modal remains present or is
  reconstructed in the active compact host until Cancel or Discard resolves
  that exact decision. Observed: no visible dialog, the window remains open,
  and the dirty draft remains mutable.
- Control proving the repaired Quit entry path itself works before resizing:

  ```sh
  env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 QT_FATAL_WARNINGS=1 \
    /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/responsive_close_probe \
    --shortcut-only
  ```

  Exit 0: `observed quitShortcutDialog= true windowVisible= true dirty= true`.

The committed lifecycle test does not cover this path. Its responsive case
starts a **navigation** departure, whose pending state is derived from the
shared dirty model; its top-level-close case only exercises Cancel without a
host switch. Thus the broader testing-harness claim is not evidence for the
application-close state introduced by the repair.

#### P2-2 — The claimed private-repository boundary negative control accepts a forbidden private header

- Contract and test locations:
  - `docs/wiki/architecture/module-boundaries.md:87` forbids private repository
    headers in `src/apps/settings/customize`.
  - `docs/wiki/apps/customize-settings.md:107-110` and
    `docs/wiki/development/testing-harness.md:804-808` claim the poisoned row
    rejects a private-repository dependency.
  - `tests/apps/settings/customize/check_boundary.cmake:15-31` checks selected
    shell/compositor strings and special-cases `QDBusConnection`, but has no
    private-header/path matcher.
  - `tests/apps/settings/customize/check_boundary_negative.cmake:9-22` plants
    only a combined LayerShellQt plus QDBus poison, so it cannot prove the
    separate private-repository prohibition.
- Exact reproduction:

  `/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/boundary-private-poison/private_header.cpp`
  contains only the scratch comment and:

  ```cpp
  #include "src/shell_customization/src/layout_editing_repository_p.h"
  ```

  Run:

  ```sh
  cmake \
    -DSCAN_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/boundary-private-poison \
    -P /home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas-k3-review/tests/apps/settings/customize/check_boundary.cmake
  ```

  Observed: exit 0 with no diagnostic. Expected: nonzero rejection of the
  private `_p.h` dependency. Current candidate sources do not contain such an
  include, so this is a missing negative control/documentation overclaim, not
  a live module-boundary crossing. It is P2 under the brief's explicit
  missing-negative-control severity rule.

### P3

None.

The three prior P3s are closed in their original scope:

- the boundary positive scan now recursively discovers all 21 route-owned C++,
  header, and QML files, with the Settings1 composition exception named;
- the stale constant `selected` projection was removed and the model test
  enforces one live selection property; and
- a dirty **navigation** decision reconstructs its prompt across the responsive
  host switch, as the warning-fatal lifecycle row demonstrates.

## Repair-question answers

1. **Prior P2-1 is closed.** `CustomizeActionBar.qml:76` uses
   `T.Overlay.overlay`; the warning-fatal page row passes in Debug and Release
   and asserts both dialog coordinates within one pixel of the window center.
2. **Prior P2-2 is closed for initial close routing and Cancel.** Both ordinary
   close and Ctrl+Q reach `Main.qml`'s `onClosing`, show the dirty confirmation,
   and Cancel keeps the window/draft. The Settings Center selector is 9/9 green
   in both profiles. P2-1 above is a narrower responsive-state defect in that
   repair.
3. **Prior P3 code defects are closed, but boundary proof is still overstated.**
   Registry/component changes remain additive: Customize is appended after
   Network, target dependencies are appended, and the install RPATH adds only
   the Customize entry. No audio route entry is removed, reordered, or
   preclaimed; the pending audio repair can extend the same registries.
4. **Required evidence rerun.** Customize is 6/6, the full customization/editor
   selector is 17/17, and Settings Center is 9/9 in both Debug and Release.
   All requested static gates pass.

## Exact commands and results

All commands below ran from the exact-candidate worktree unless an absolute
test directory is shown.

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

Both exited 0 from initially empty build directories. CMake emitted the known
dependency-root/runtime-path warnings outside candidate scope.

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
  qindaqt_panel_editing_tests \
  qindaqt_applet_editing_tests \
  qindaqt_preview_history_tests \
  qindaqt_coordinator_lease_tests \
  qindaqt_editor_query_tests \
  qindaqt_customize_editor_intent_tests \
  qindaqt_customize_editor_gesture_tests \
  qindaqt_customize_editor_session_tests \
  qindaqt_customize_editor_dirty_state_tests \
  qindaqt_customize_editor_persistence_tests \
  qindaqt_customize_editor_accessibility_tests
```

Debug: exit 0, 519/519 Ninja steps. Release: exit 0, 519/519 Ninja steps.

### Tests

```sh
ctest --test-dir <ROOT>/debug \
  -R '^qindaqt\.settings-customize-' --output-on-failure --no-tests=error
ctest --test-dir <ROOT>/release \
  -R '^qindaqt\.settings-customize-' --output-on-failure --no-tests=error
```

Both exit 0, 6/6 passed. The page and lifecycle rows carry
`QT_FATAL_WARNINGS=1` in their committed CTest environment.

```sh
ctest --test-dir <ROOT>/debug \
  -R 'customiz' --output-on-failure --no-tests=error
ctest --test-dir <ROOT>/release \
  -R 'customiz' --output-on-failure --no-tests=error
```

Both exit 0, 17/17 passed: five `shell-customization` rows, six
`customize-editor` rows, and six Settings Customize rows.

```sh
ctest --test-dir <ROOT>/debug \
  -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' \
  --output-on-failure --no-tests=error
ctest --test-dir <ROOT>/release \
  -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' \
  --output-on-failure --no-tests=error
```

Both exit 0, 9/9 passed.

Additional warning-fatal navigation control:

```sh
env QT_FATAL_WARNINGS=1 ctest --test-dir <ROOT>/debug \
  -R '^qindaqt\.settings-navigation-page$' --output-on-failure --no-tests=error
env QT_FATAL_WARNINGS=1 ctest --test-dir <ROOT>/release \
  -R '^qindaqt\.settings-navigation-page$' --output-on-failure --no-tests=error
```

Both exit 0, 1/1 passed.

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
  5a411b52da08f09a58be351daa47b60b1ef51a6c
```

Both exit 0. No JSON file changed between base and candidate, so no
`python3 -m json.tool` invocation applies.

## Unavailable or prohibited coverage

No mandated gate was unavailable. Per the lane prohibition, no `tests/session`
nested-compositor row, host D-Bus service, hardware, uinput, or network was run.

## Verdict

The original P2s and P3s were substantially repaired, but ACCEPT requires zero
P2 findings. The remaining responsive close-state defect and missing private-
repository negative control require another bounded repair/recheck.

VERDICT REJECT P0/P1/P2/P3=0/0/2/0
