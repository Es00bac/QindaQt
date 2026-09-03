# Lynn Conway — third Customize canvas repair handoff

- Timestamp: `2026-09-03T00:19:36-06:00`
- Exact candidate commit: `2500a3d71343f238ce30fd0d98f5cc2aa9a95809`
- Candidate tree: `f0a329d08c84b2a9a16ac9d9212d15dd608d3b94`
- Candidate parent: `83a9ecc048b978e1998052b2ab15ee514656abfb`
- Exact rejected product ancestor: `e537fc9ad4c6bcf118cc1ec8646cf3211cd96e74`
- Exact base: `03dc71e6dfd06b6e30f8f86ac354fe24684f497a`
- Branch: `worker/customize-settings-canvas`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas`

## Outcome

The Customize C++ include scan is now a positive boundary. It accepts only
Qt/system angle includes, existing route-local headers confined by resolved
real path, and canonical public `qindaqt/` prefixes for the route and its named
dependencies. It rejects every `src/` path and parent segment before
classification, disallows unlisted `qindaqt/` prefixes, and detects shortened
spellings of any source-tree file by matching against the repository inventory.

The hostile wrapper independently plants Adele Goldstine's exact
`src/apps/settings_center/settings_route_registry.h` include and a
`../settings_center/settings_route_registry.h` escape. It requires both checker
invocations to fail and verifies each diagnostic names the planted include, so
an unrelated failure cannot satisfy the control. The Customize route and
testing-harness pages state this same allowlist and two-control contract.

## Changed product paths

- `docs/wiki/apps/customize-settings.md`
- `docs/wiki/development/testing-harness.md`
- `tests/apps/settings/customize/check_boundary.cmake`
- `tests/apps/settings/customize/check_boundary_negative.cmake`

## Verification evidence

The real route, registered poison wrapper, and Adele's exact scratch inputs
were exercised directly:

```sh
cmake -DSOURCE_ROOT=/home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas \
  -P tests/apps/settings/customize/check_boundary.cmake
cmake \
  -DPOISON_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/scratch/boundary-poison \
  -DCHECK_SCRIPT=/home/cabewse/work_SPaC3/container-wm-workers/customize-settings-canvas/tests/apps/settings/customize/check_boundary.cmake \
  -P tests/apps/settings/customize/check_boundary_negative.cmake
cmake \
  -DSCAN_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/boundary-own-hostile \
  -P tests/apps/settings/customize/check_boundary.cmake
cmake \
  -DSCAN_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/review-customize-r2-codex/scratch/boundary-private-poison \
  -P tests/apps/settings/customize/check_boundary.cmake
```

The real route and registered wrapper exited 0. Adele's sibling Settings
Center scratch and the prior exact private-header scratch each exited 1 with
the planted include named as a non-public repository header. A shell assertion
required both expected nonzero statuses and exited 0.

A scratch-only `#include <settings_route_registry.h>` probe under the assigned
build root also returned checker exit 1, and its outer expected-failure
assertion exited 0. This directly verifies that dropping the repository path
prefix cannot evade the source-tree inventory match. The scratch source was
removed after the probe.

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

Debug and Release both exited 0. CMake emitted the known dependency-prefix
runtime-path warnings outside candidate scope.

For each profile:

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_settings_customize_model_tests \
  qindaqt_settings_customize_page_tests \
  qindaqt_settings_customize_window_lifecycle_tests \
  qindaqt-settings
```

Debug and Release both exited 0 under strict warnings.

The boundary rows were run independently after the final checker edit:

```sh
ctest --test-dir <ROOT>/<profile> \
  -R '^qindaqt\.settings-customize-boundary(-poison)?$' \
  --output-on-failure --no-tests=error
```

Debug: exit 0, 2/2 passed. Release: exit 0, 2/2 passed.

The complete required selector was then rerun:

```sh
ctest --test-dir <ROOT>/<profile> \
  -R '^qindaqt\.settings-customize-' \
  --output-on-failure --no-tests=error
```

Debug: exit 0, 6/6 passed. Release: exit 0, 6/6 passed. This includes model,
warning-fatal page, warning-fatal window lifecycle, positive boundary, hostile
boundary, and relocated installed-route rows.

Static gates after the final product edit:

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/site
./tools/check-source-shape
git diff --check
```

All exited 0. Documentation validated 117 Markdown documents and navigation.
MkDocs completed its strict build. Source shape checked 1,787 files and emitted
only the two pre-existing out-of-lane decomposition-review warnings. No JSON
changed, so no JSON parser gate applies.

Identity/ownership checks confirmed candidate `2500a3d71343f238ce30fd0d98f5cc2aa9a95809`,
tree `f0a329d08c84b2a9a16ac9d9212d15dd608d3b94`, parent
`83a9ecc048b978e1998052b2ab15ee514656abfb`, the four sorted product paths above,
and that the exact base is an ancestor. `git show --check` exited 0.

## Bounded caveats

- This repair supplies static source-policy, offscreen, fake-transport,
  temporary-store, and relocated-package evidence only.
- Per lane prohibition, no nested compositor/session row, host D-Bus service,
  hardware, uinput, or network was contacted.
- Live shell binding, always-hidden reveal behavior, live AT-SPI, installed
  session behavior, and the rendered desktop matrix remain later slices.

## Requested next action

Please route exact candidate `2500a3d71343f238ce30fd0d98f5cc2aa9a95809`
back to **Adele Goldstine (OpenAI Codex)** for an independent exact recheck,
then manager integration if accepted.
