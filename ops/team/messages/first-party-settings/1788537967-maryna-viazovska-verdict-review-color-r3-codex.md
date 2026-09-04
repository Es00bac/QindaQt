# Maryna Viazovska — independent Color Settings route repair recheck

- Persona: Maryna Viazovska, independent Settings reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Exact candidate SHA: `85c8e8c9e54db9c820f8f1934996292219a43dd5`
- Tree SHA: `eed3fd919ffc15ce87860d10c332cf8769639988`
- Parent SHA: `0862d2c4eff12cb8ea435ff89a6d43ee6be5a658`
- Base SHA: `b971b43881fcef18980acec03c4e43e56ef9db2a`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/color-settings-route-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex`

## Verdict summary

ACCEPT. The repair closes the prior P1. With both D-Bus addresses set to
nonexistent sockets, display variables removed, and HOME/XDG roots redirected,
the complete `^qindaqt\.settings-` selector passes 55/55 in Debug and Release.
The three formerly aborting warning-fatal host rows now pin both bus addresses
in their registered CTest environments. Verbose execution shows the expected
Settings1 startup failure only as categorized `QINFO`, while Color exposes its
existing unavailable/degraded presentation truth and unrelated Customize,
Bluetooth, and navigation behavior continues to pass.

The exact rejected candidate `252b2fd7d6d590178b33135af9d64123b1acf6cf`
was built in a detached scratch worktree under the assigned build root. Under
the same unreachable-bus environment, all three affected rows abort on the old
Color `QWARN` (0/3, CTest exit 8). This is a non-vacuous negative control for
the source fix and the new test-environment pins.

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Contract and source review

- `src/apps/settings/color/color_route_composition.cpp:22-23,80-103` confines
  the unavailable Settings1 diagnostic to the
  `qindaqt.settings.color.composition` category at `QtInfoMsg`; provisioning
  failures remain warnings because they are genuine local filesystem faults.
- `src/apps/settings/color/color_settings_model.cpp:66-104,118-133,221-247`
  keeps unavailable/degraded/stale truth distinct and closes assignment
  admission unless exact Display1 lineage and a ready Settings1 document are
  present. Demoting the expected startup diagnostic does not fabricate
  authority or open a mutation path.
- `src/apps/settings_center/Main.qml:26-30` still evaluates the engine-scoped
  route compositions for the process lifetime. The strict no-bus selector and
  four-row verbose run prove that the Color singleton's unavailable transport
  cannot make unrelated pages depend on a reachable host bus or turn expected
  degraded truth into a warning-fatal abort.
- `tests/apps/settings/customize/CMakeLists.txt:113-121`,
  `tests/apps/settings/bluetooth/CMakeLists.txt:118-124`, and
  `tests/apps/settings_center/CMakeLists.txt:104-116` register both D-Bus
  addresses as nonexistent for the three affected warning-fatal host rows.
  `tests/apps/settings/color/CMakeLists.txt:11-98` does the same for the five
  executable Color rows; the remaining boundary rows are source-only and the
  installed-route script independently supplies absent bus sockets.
- The full selector retained 7/7 Customize, 7/7 Bluetooth, 8/8 Color, and the
  Settings Center registry/controller/navigation/application rows in each
  profile. Existing model, apply, page, navigation, boundary poison, installed
  package, exact-lineage, conflict/no-replay, accessibility, and responsive
  checks therefore remain live rather than being replaced by the repair.
- The change is additive at the shared CTest registrations and does not cross
  the documented Settings Center, Color, Customize, Bluetooth, Display1, or
  Settings1 module boundaries. No host display, live bus service, hardware,
  uinput, network, or nested-compositor row was used.

## Commands and results

### Identity and tree cleanliness

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git merge-base b971b43881fcef18980acec03c4e43e56ef9db2a HEAD
git merge-base --is-ancestor b971b43881fcef18980acec03c4e43e56ef9db2a HEAD
git status --porcelain=v1
```

Exit 0. The values match the header, the base is an ancestor, and the candidate
worktree was empty before review.

### Candidate configure

Run once with `CMAKE_BUILD_TYPE=Debug` and the Debug build directory, and once
with `CMAKE_BUILD_TYPE=Release` and the Release build directory:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both exited 0. CMake emitted the existing mixed-prefix runtime-path warnings;
configuration and generation completed.

### Focused and adjacent candidate build

Run against each candidate profile directory:

```sh
cmake --build <profile-build> --parallel 3 --target \
  qindaqt-settings qindaqt_settings_navigation_page_test \
  qindaqt_settings_customize_window_lifecycle_tests \
  qindaqt_bluetooth_window_close_tests \
  qindaqt_color_settings_model_tests qindaqt_color_settings_apply_tests \
  qindaqt_color_route_composition_tests qindaqt_color_page_tests \
  qindaqt_color_navigation_page_tests qindaqt-desktop-session-probe \
  qindaqt_global_menu_qmlplugin qindaqt_shell_launcher_qmlplugin
```

Debug and Release both exited 0; each incremental build completed at 33/33
Ninja actions after regenerating the affected graph.

### Registered environment inspection

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/debug \
  -N -V -R '^(qindaqt\.settings-(navigation-page|customize-window-lifecycle|bluetooth-window-close|color-(model|apply|composition|page|navigation-page|boundary|boundary-poison|installed-route)))$'
```

Exit 0; 11 rows enumerated. The three affected host rows and all five
executable Color rows showed
`DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent` and
`DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`. Warning-fatal presentation
rows also showed the expected offscreen/software settings.

### Exact full-selector P1 reproduction

For each profile, the expanded command used its corresponding build and XDG
directory (`debug` shown; `release` substituted in both places):

```sh
mkdir -p /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/{home,config,data,cache,runtime}
chmod 700 /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/runtime
env -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/home \
  XDG_CONFIG_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/config \
  XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/data \
  XDG_CACHE_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/cache \
  XDG_RUNTIME_DIR=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-unreachable-debug/runtime \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/debug \
    -R '^qindaqt\.settings-' --output-on-failure --no-tests=error
```

- Debug: exit 0, 55/55 passed, 105.55 seconds.
- Release: exit 0, 55/55 passed, 102.20 seconds.
- No SIGABRT occurred.

The focused verbose control was:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-verbose-debug/home \
  XDG_CONFIG_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-verbose-debug/config \
  XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-verbose-debug/data \
  XDG_CACHE_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-verbose-debug/cache \
  XDG_RUNTIME_DIR=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/xdg-verbose-debug/runtime \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/debug -V \
    -R '^(qindaqt\.settings-(navigation-page|customize-window-lifecycle|bluetooth-window-close|color-composition))$' \
    --no-tests=error
```

Exit 0, 4/4. Every disconnected Color diagnostic was `QINFO` in the named
category; the rows reported respectively 5/5, 8/8, 4/4, and 6/6 QtTest
functions with no warning or abort.

### Exact `252b2fd` negative control

```sh
git worktree add --detach \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/control-252-src \
  252b2fd7d6d590178b33135af9d64123b1acf6cf
```

Exit 0; the detached scratch worktree resolved to the exact rejected SHA and
was clean. Attempting the current system-KWin cache correctly exited 1 because
that historical commit requires exact KWin 6.6.5. It was then configured with
the preserved compatible cache:

```sh
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/control-252-src \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/control-252-debug-665 \
  -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/control-252-debug-665 \
  --parallel 3 --target qindaqt_settings_navigation_page_test \
  qindaqt_settings_customize_window_lifecycle_tests \
  qindaqt_bluetooth_window_close_tests
```

Configure exited 0; build exited 0 at 749/749 actions. The exact failure
control was:

```sh
ulimit -c 0
env -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/control-252-xdg-debug/home \
  XDG_CONFIG_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/control-252-xdg-debug/config \
  XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/control-252-xdg-debug/data \
  XDG_CACHE_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/control-252-xdg-debug/cache \
  XDG_RUNTIME_DIR=/home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/control-252-xdg-debug/runtime \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/control-252-debug-665 \
    -R '^qindaqt\.settings-(customize-window-lifecycle|bluetooth-window-close|navigation-page)$' \
    --output-on-failure --no-tests=error
```

Observed: exit 8, 0/3 passed. All three processes received SIGABRT immediately
after `qindaqt-settings: color Settings1 client unavailable: settings session
D-Bus is not connected` was emitted as `QWARN` from
`color_route_composition.cpp:88`. Expected for the negative control: failure;
the repaired candidate instead passes these rows.

### Safe DesktopVirtual rows

For both profile builds, with display/Wayland removed, both buses set to
nonexistent sockets, and profile-specific HOME/XDG directories under the
assigned build root:

```sh
ctest --test-dir <profile-build> \
  -R '^desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)$' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 3/3 passed (11.29 seconds).
- Release: exit 0, 3/3 passed (10.98 seconds).

The stage-closure row was also rerun with `-V` in each profile. Debug and
Release each passed 1/1 and reported 37 ELF files, 423 `DT_NEEDED` entries,
17 QML modules, staged application load, and effective missing-library and
missing-QML-module negative controls.

### Static and JSON gates

```sh
./tools/validate-docs
```

Exit 0; validated 136 Markdown documents and `mkdocs.yml` navigation.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-color-r2-codex/site
```

Exit 0; documentation built in 1.76 seconds.

```sh
./tools/check-source-shape
```

Exit 0; checked 2324 source files, skipped 0. Only the existing decomposition
review warnings were emitted; no candidate path introduced one.

```sh
git diff --check
git diff --check b971b43881fcef18980acec03c4e43e56ef9db2a..HEAD
git diff --name-only b971b43881fcef18980acec03c4e43e56ef9db2a..HEAD -- '*.json'
```

All exited 0. The JSON enumeration was empty, so there was no changed JSON file
to pass to `python3 -m json.tool`.

## Final state

`git rev-parse HEAD` remained
`85c8e8c9e54db9c820f8f1934996292219a43dd5`; `git status --porcelain=v1`
remained empty for both the candidate and detached negative-control worktrees.
No product file was edited, committed, amended, or rebased. Scratch build and
negative-control artifacts are confined to the assigned build root.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
