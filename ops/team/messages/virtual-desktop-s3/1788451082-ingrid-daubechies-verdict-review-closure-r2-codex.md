# Ingrid Daubechies — independent exact-candidate repair recheck

- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Exact candidate SHA: `91377acf822155b241fbb2474bec64a01069b6dc`
- Tree SHA: `40f243e552be1bf76b6ed25cdf794900bab7ab12`
- Parent SHA: `7bae40ddf496aa034e5ced3682dec809662f8aac`
- Base SHA merged before repair: `b971b43881fcef18980acec03c4e43e56ef9db2a`
- Rejected product ancestor / merge base: `99b06199cc256b3a76c1a6c0449690282dd3b2b6`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/desktop-stage-closure-codex-review`
- Reviewed ranges: `git diff 99b0619..91377ac` and repair-only `git diff 7bae40dd..91377ac`

## Findings ledger

### P0

None.

### P1

None. The prior P1 is closed: the repaired registered row's executable and
arguments, replayed against the preserved exact-`99b0619` DesktopVirtual stage,
exit 1 at the intended assertion:

```text
DesktopVirtual stage closure failed: staged QML module
QindaQt.SettingsApp.Audio has no qmldir
```

The exact descendant passes the same registered row and keeps the staged
Settings Center live offscreen with display, Wayland, loader, and both bus
addresses absent or confined.

### P2

None.

### P3

None.

## Repair and merge review

- The repair commit changes only the owning testing-harness page and four
  focused stage-closure test paths. It derives Settings imports from
  `src/apps/settings_center/Main.qml`, permits only the two verified static
  embedded modules (`Customize` and `PowerBackend`), rejects stale exemptions,
  probes the staged Settings Center, and registers a Power-qmldir negative
  control.
- `7bae40dd` is the merge of the lane ancestry with `b971b438` main.
  `git show --remerge-diff 7bae40dd -- <lane paths>` printed only the commit
  header and no resolution diff, so there is no manual conflict-resolution
  delta to conceal a lane change. The split
  `DesktopSessionRouteStaging.cmake` remains included inside the guarded
  DesktopVirtual block and stages Appearance, Display, Network, Audio,
  Bluetooth, and Power.
- No nested compositor/session row, host bus service, hardware, uinput, or
  network path was run.

## Historical-gap reproductions

Scratch root:
`/home/cabewse/work_SPaC3/builds/qindaqt/review-closure-codex/reproductions/closure-r2.pF2u3F`.

### Rejected predecessor replay

Before reconfiguring the assigned build roots, the preserved Debug
`desktop-session-stage` from the prior exact-`99b0619` review was copied beneath
the scratch root. `ctest -N -V -R '^desktop\.virtual\.stage-closure$'` printed
the repaired registered command. I invoked that exact test executable and
argument set with only `--cmake` replaced by the scratch installer replay
wrapper and `--stage-root` replaced by a fresh child of the same authenticated
build root:

```text
PYTHONDONTWRITEBYTECODE=1 env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  /usr/bin/python3.14 \
  /home/cabewse/work_SPaC3/container-wm-workers/desktop-stage-closure-codex-review/tests/session/test_desktop_session_stage_closure.py \
  --cmake <SCRATCH>/replay_99_cmake --readelf /usr/bin/readelf \
  --build-root <ROOT>/debug \
  --source-root /home/cabewse/work_SPaC3/container-wm-workers/desktop-stage-closure-codex-review \
  --stage-root <ROOT>/debug/tests/session/replay-99b0619-stage \
  --bin-directory bin --lib-directory lib64 --qml-directory lib64/qt6/qml \
  --qml-source src/shell/qml/BuiltinAppletContent.qml \
  --qml-source src/shell/qml/AppletChip.qml \
  --qml-source src/shell/qml/PanelAppletRow.qml \
  --qml-source src/shell/qml/PanelAppletColumn.qml \
  --qml-source src/shell/qml/PanelContent.qml \
  --qml-source src/shell/qml/RuntimePanel.qml \
  --qml-source src/apps/settings_center/Main.qml \
  --embedded-qml-module QindaQt.SettingsApp.Customize \
  --embedded-qml-module QindaQt.SettingsApp.PowerBackend \
  --system-library-directory /usr/lib/gcc/x86_64-pc-linux-gnu/15 \
  --system-library-directory /usr/lib64 --system-library-directory /lib64 \
  --system-library-directory /usr/x86_64-pc-linux-gnu/lib \
  --system-library-directory /usr/lib --system-library-directory /lib \
  --system-library-directory /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-arch-665/root/usr/lib \
  --system-library-directory /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-kf6-prefix/lib64 \
  --required-shell-library libqindaqt_controls_qml.so \
  --required-shell-library libqindaqt_global_menu_qml.so \
  --required-shell-library libqindaqt_shell_launcher_qml.so \
  --negative-library libqindaqt_global_menu_qml.so \
  --negative-qml-module QindaQt.SettingsApp.Power --configuration Debug
# exit 1, expected; assertion named missing QindaQt.SettingsApp.Audio qmldir
```

The source arguments above were passed as the absolute worktree paths printed
by CTest; they are shortened only for line readability in this report.

### Linked Global Menu staging omission

`git archive 91377ac` created `<SCRATCH>/lib-source`; the sole mutation removed
`qindaqt_global_menu_qml` from the library-destination `install(TARGETS ...)`
list in `tests/session/DesktopVirtualAppletModules.cmake` while retaining its
QML-module staging.

```text
cmake -S <SCRATCH>/lib-source -B <SCRATCH>/lib-build -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0

cmake --build <SCRATCH>/lib-build --parallel 3 \
  --target qindaqt-desktop-session-probe qindaqt-panel-visibility-session-probe
# exit 0; 1388/1388 Ninja actions

env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  ctest --test-dir <SCRATCH>/lib-build \
  -R '^desktop\.virtual\.stage-closure$' --output-on-failure --no-tests=error
# exit 8, expected; 0/1 passed
# qindaqt-shell cannot resolve DT_NEEDED libqindaqt_global_menu_qml.so
```

### Power route removed from `DesktopSessionRouteStaging.cmake`

`git archive 91377ac` independently created `<SCRATCH>/qml-source`; the sole
mutation removed lines 203-232, the complete Power target/metadata/QML install
block, from `tests/session/DesktopSessionRouteStaging.cmake`.

```text
cmake -S <SCRATCH>/qml-source -B <SCRATCH>/qml-build -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0

cmake --build <SCRATCH>/qml-build --parallel 3 \
  --target qindaqt-desktop-session-probe qindaqt-panel-visibility-session-probe
# exit 0; 1388/1388 Ninja actions

env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  ctest --test-dir <SCRATCH>/qml-build \
  -R '^desktop\.virtual\.stage-closure$' --output-on-failure --no-tests=error
# exit 8, expected; 0/1 passed
# staged QML module QindaQt.SettingsApp.Power has no qmldir
```

Both failures are produced by the registered row before either mutation can be
mistaken for a passing negative-control run.

## Exact-candidate commands and results

`<ROOT>` is
`/home/cabewse/work_SPaC3/builds/qindaqt/review-closure-codex`.

### Identity and cleanliness

```text
git rev-parse HEAD
# exit 0; 91377acf822155b241fbb2474bec64a01069b6dc
git rev-parse HEAD^{tree}
# exit 0; 40f243e552be1bf76b6ed25cdf794900bab7ab12
git rev-parse HEAD^
# exit 0; 7bae40ddf496aa034e5ced3682dec809662f8aac
git merge-base 99b0619 91377ac
# exit 0; 99b06199cc256b3a76c1a6c0449690282dd3b2b6
git status --porcelain
# exit 0; empty before and after review
```

### Configure and focused builds

```text
cmake -S . -B <ROOT>/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0

cmake -S . -B <ROOT>/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0

cmake --build <ROOT>/debug --parallel 3 \
  --target qindaqt-desktop-session-probe qindaqt-panel-visibility-session-probe
# completed; confirming incremental invocation exit 0, no work to do

cmake --build <ROOT>/release --parallel 3 \
  --target qindaqt-desktop-session-probe qindaqt-panel-visibility-session-probe
# exit 0; 69/69 incremental Ninja actions
```

Both configure runs emitted the existing CMake RPATH/dependent-library search
warnings and completed successfully.

### Focused and adjacent tests

```text
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  ctest --test-dir <ROOT>/debug \
  -R 'desktop\.virtual\.(stage-closure|package-contract|sandbox-unit)' \
  --output-on-failure --no-tests=error
# exit 0; 3/3 passed

env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  ctest --test-dir <ROOT>/release \
  -R 'desktop\.virtual\.(stage-closure|package-contract|sandbox-unit)' \
  --output-on-failure --no-tests=error
# exit 0; 3/3 passed

env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  ctest --test-dir <ROOT>/debug \
  -R '^desktop\.virtual\.stage-closure$' -V --no-tests=error
# exit 0; 1/1 passed; 35 ELF files, 401 DT_NEEDED entries, 15 QML
# modules, both staged applications load, and both negative controls pass

PYTHONDONTWRITEBYTECODE=1 \
  PYTHONPATH=/home/cabewse/work_SPaC3/container-wm-workers/desktop-stage-closure-codex-review/tests/session \
  TMPDIR=<SCRATCH> python3 -m unittest -v \
  test_desktop_session_stage_closure_unit
# exit 0; 4/4 passed

ctest --test-dir <ROOT>/debug -R '^session\.python-syntax$' \
  --output-on-failure --no-tests=error
# exit 0; 1/1 passed
ctest --test-dir <ROOT>/release -R '^session\.python-syntax$' \
  --output-on-failure --no-tests=error
# exit 0; 1/1 passed
```

### Static gates

```text
./tools/validate-docs
# exit 0; 135 Markdown documents and mkdocs.yml navigation validated

/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir <ROOT>/site-r2
# exit 0

./tools/check-source-shape
# exit 0; 2301 source files checked; 0 allowlisted skips; seven reported
# decomposition warnings are non-failing and outside the repair diff

git diff --check
git diff --check 99b0619..91377ac
git diff --check 7bae40dd..91377ac
# all exit 0

python3 -m json.tool data/applet-policy/default.json
python3 -m json.tool data/applets/clipboard.json
python3 -m json.tool data/settings/schema-v2.json
python3 -m json.tool ops/team/features.json
# all exit 0; these are all JSON paths changed in 99b0619..91377ac;
# the repair-only diff changes no JSON
```

## Verdict

The repair closes the prior false-negative, the exact candidate passes every
required Debug/Release and static gate, and independent source-level mutations
of both historical staging gaps fail at the intended assertions. No P0-P3
finding remains.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
