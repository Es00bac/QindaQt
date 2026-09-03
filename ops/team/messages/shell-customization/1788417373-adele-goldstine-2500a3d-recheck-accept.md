# Adele Goldstine — independent exact-candidate third-repair recheck

- Reviewer: **Adele Goldstine** (`adele-goldstine`)
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `2500a3d71343f238ce30fd0d98f5cc2aa9a95809`
- Candidate tree SHA: `f0a329d08c84b2a9a16ac9d9212d15dd608d3b94`
- Parent SHA: `83a9ecc048b978e1998052b2ab15ee514656abfb`
- Base SHA: `03dc71e6dfd06b6e30f8f86ac354fe24684f497a`
- Repaired product ancestor: `e537fc9ad4c6bcf118cc1ec8646cf3211cd96e74`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas-k3-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex`
- Scratch root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch`

The worktree was detached at the exact candidate and `git status --porcelain`
was empty before and after review. All scratch material remained below the
assigned build root. No product path was edited, committed, amended, or
rebased.

## Verdict summary

**ACCEPT.** The third repair closes the remaining source-boundary defect. The
checker is now a positive allow-list for named public `qindaqt/` prefixes,
real-path-confined route-local headers, and non-repository angle includes. It
rejects repository `src/` forms, parent escapes, unlisted public prefixes, and
shortened names that resolve to any file in the repository source inventory.
The two registered poisons independently require the exact rejected include to
appear in each diagnostic. The earlier responsive-close and exact private-header
closures also remain sound.

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Recheck-focus answers

1. **Responsive-close closure remains valid.** The external probe passed
   wide-to-compact and compact-to-wide in Debug and Release. All four runs
   observed `closeAccepted=false`, `dialogAfterResize=true`,
   `applicationClosePending=true`, `windowVisible=true`, and `dirty=true`.
   The registered lifecycle test at
   `tests/apps/settings/customize/tst_customize_window_lifecycle.cpp:142`
   explicitly runs both directions, locates the reconstructed dialog in the
   named active host, and proves Cancel clears only the pending decision while
   retaining the visible dirty draft.
2. **Boundary closure is complete for the requested attack surface.** The real
   route passes. The registered negative control at
   `tests/apps/settings/customize/check_boundary_negative.cmake:9` plants both
   the exact sibling Settings Center include and the parent escape, requires
   each scan to fail independently, and verifies the diagnostic names the
   planted include. My direct sibling, direct parent-escape, and prior private
   `layout_editing_repository_p.h` probes all exited 1 at
   `tests/apps/settings/customize/check_boundary.cmake:113` with the expected
   include named. The allow-list and repository inventory are visible at
   `check_boundary.cmake:18-38,58-110` and match the contract at
   `docs/wiki/apps/customize-settings.md:107`.
3. **No runtime behavior moved in the repair descendant.** Relative to
   `e537fc9`, product changes are limited to two boundary scripts and two
   aligned wiki pages; the other descendant changes are coordination records.
   `src/apps/settings_center` and its tests are byte-identical to the rejected
   product ancestor. Relative to the base, the Settings Center changes append
   Customize after Network, add its links/imports/RPATH/host branch, and extend
   existing route tests without deleting or reordering an existing route.
4. **All required executable and static evidence passes in both build types.**
   Customize passed 6/6, warning-fatal page/lifecycle passed 2/2,
   customization/domain passed 17/17, and Settings Center passed 9/9 in both
   Debug and Release.

## Exact commands and results

### Identity, ancestry, scope, and cleanliness

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git status --porcelain
git merge-base --is-ancestor \
  03dc71e6dfd06b6e30f8f86ac354fe24684f497a \
  2500a3d71343f238ce30fd0d98f5cc2aa9a95809
git diff --quiet \
  e537fc9ad4c6bcf118cc1ec8646cf3211cd96e74 \
  2500a3d71343f238ce30fd0d98f5cc2aa9a95809 \
  -- src/apps/settings_center tests/apps/settings_center
```

All exited 0 where applicable. The three identities exactly matched the header;
status was empty before and after review. The base is an ancestor, and no
Settings Center source/test changed after `e537fc9`.

### Configure

For `<profile>/<type>` equal to `debug/Debug` and `release/Release`:

```sh
cmake -S . \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/<profile> \
  -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=<type> -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Debug and Release exited 0. CMake emitted only the known dependency-prefix
runtime-path warnings outside this candidate's scope.

### Focused and adjacent build

For each profile:

```sh
cmake --build \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/<profile> \
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

Debug and Release both exited 0 under strict warnings.

### Boundary probes

```sh
cmake \
  -DSOURCE_ROOT=/home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas-k3-review \
  -P tests/apps/settings/customize/check_boundary.cmake

cmake \
  -DPOISON_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/registered-repair3-poison \
  -DCHECK_SCRIPT=/home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas-k3-review/tests/apps/settings/customize/check_boundary.cmake \
  -P tests/apps/settings/customize/check_boundary_negative.cmake
```

Both exited 0: the route was accepted and the wrapper observed independent,
correctly attributed rejection of both registered poisons.

```sh
cmake \
  -DSCAN_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/boundary-own-hostile \
  -P tests/apps/settings/customize/check_boundary.cmake

cmake \
  -DSCAN_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/boundary-parent-escape \
  -P tests/apps/settings/customize/check_boundary.cmake

cmake \
  -DSCAN_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/boundary-private-poison \
  -P tests/apps/settings/customize/check_boundary.cmake
```

Each exited 1 as expected, naming respectively
`src/apps/settings_center/settings_route_registry.h`,
`../settings_center/settings_route_registry.h`, and
`src/shell_customization/src/layout_editing_repository_p.h` as non-public
repository headers. The nonzero results are the expected success condition of
these direct negative probes.

### Responsive-close external probe

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

All four exited 0 with the preserved close/dialog/pending/window/dirty truth
reported above.

### Registered tests

For each profile:

```sh
ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/<profile> \
  -R '^qindaqt\.settings-customize-' \
  --output-on-failure --no-tests=error
```

Debug and Release: exit 0, 6/6 passed.

```sh
env QT_FATAL_WARNINGS=1 ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/<profile> \
  -R '^qindaqt\.settings-customize-(page|window-lifecycle)$' \
  --output-on-failure --no-tests=error
```

Debug and Release: exit 0, 2/2 passed.

```sh
ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/<profile> \
  -R 'customiz' --output-on-failure --no-tests=error
```

Debug and Release: exit 0, 17/17 passed.

```sh
ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/<profile> \
  -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' \
  --output-on-failure --no-tests=error
```

Debug and Release: exit 0, 9/9 passed.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/site
./tools/check-source-shape
git diff --check
git diff --check \
  03dc71e6dfd06b6e30f8f86ac354fe24684f497a \
  2500a3d71343f238ce30fd0d98f5cc2aa9a95809
git show --check --oneline --no-patch \
  2500a3d71343f238ce30fd0d98f5cc2aa9a95809
```

All exited 0. Documentation validated 117 Markdown files and navigation;
MkDocs completed its strict build; source shape checked 1,787 files and emitted
only the two pre-existing out-of-candidate decomposition-review warnings. No
JSON file changed from base to candidate, so no `python3 -m json.tool` gate
applies.

## Unavailable or prohibited coverage

No mandated gate was unavailable. Per the lane prohibition, no `tests/session`
nested-compositor row, host D-Bus service, hardware, uinput, or network was run.

## Verdict

The requested repair is closed with no blocking or nonblocking defect found.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
