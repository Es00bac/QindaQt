# Ingrid Daubechies — independent exact-candidate review

- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Candidate SHA: `99b06199cc256b3a76c1a6c0449690282dd3b2b6`
- Tree SHA: `436b39949843520975eefa8a26d2ca70bbd6ebbb`
- Parent SHA: `e51372a49b3493435246de663d04a712fa5d78f4`
- Base SHA: `e51372a49b3493435246de663d04a712fa5d78f4`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/desktop-stage-closure-codex-review`
- Reviewed diff: `git diff e51372a..99b06199cc256b3a76c1a6c0449690282dd3b2b6`

## Findings ledger

### P0

None.

### P1

1. **The new guards pass the exact pre-`197f104` DesktopVirtual stage even though the staged Settings Center is unloadable.**

   Contract and source evidence:

   - `src/apps/settings_center/Main.qml:11-14` imports
     `QindaQt.SettingsApp.Audio`, `Bluetooth`, `Power`, and
     `PowerBackend`.
   - `tests/session/DesktopSessionTests.cmake:40-45` and
     `tests/session/DesktopSessionTests.cmake:162-305` stage and deploy only
     the Appearance, Display, and Network Settings route modules.
   - `tests/session/DesktopPackageTests.cmake:3-17` supplies only the Network
     artifact names to the package-contract row, and
     `tests/session/test_desktop_session_package.py:57-62` authenticates only
     that Network package.
   - `tests/session/DesktopPackageTests.cmake:64-69` supplies only shell/panel
     QML sources to the new closure verifier; it never supplies
     `SettingsApp/Main.qml`. Consequently, Settings route imports are outside
     the row's derived QML closure.
   - `git merge-base --is-ancestor 197f104
     99b06199cc256b3a76c1a6c0449690282dd3b2b6` exited 1. The exact candidate is
     the state before the Settings route staging commit. `git show 197f104`
     confirms that commit adds the Audio, Bluetooth, and Power targets,
     metadata, and QML payload blocks to `DesktopSessionTests.cmake`.
   - The normative harness text at
     `docs/wiki/development/testing-harness.md:2209-2218` requires the
     complete import closure of every Settings route compiled into
     `Main.qml`. Its current enumeration of only Appearance, Display, and
     Network is itself stale relative to `Main.qml`, and the candidate leaves
     that overclaim in place.

   Exact reproduction:

   1. Both required candidate selectors passed:

      ```text
      ctest --test-dir <ROOT>/debug -R 'desktop\.virtual\.(stage-closure|package-contract|sandbox-unit)' --output-on-failure --no-tests=error
      # exit 0; 3/3 passed

      ctest --test-dir <ROOT>/release -R 'desktop\.virtual\.(stage-closure|package-contract|sandbox-unit)' --output-on-failure --no-tests=error
      # exit 0; 3/3 passed
      ```

   2. The resulting exact Debug package stage contains only
      `SettingsApp/Appearance`, `SettingsApp/Display`, and
      `SettingsApp/Network` qmldirs. Starting its Settings Center offscreen
      with empty ambient state and deliberately nonexistent bus addresses:

      ```sh
      env -i \
        HOME=<ROOT>/debug/tests/session/desktop-session-stage \
        LC_ALL=C.UTF-8 PATH=/usr/bin:/bin \
        QML_IMPORT_PATH=<ROOT>/debug/tests/session/desktop-session-stage/lib64/qt6/qml \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
        XDG_DATA_DIRS=<ROOT>/debug/tests/session/desktop-session-stage/share \
        XDG_RUNTIME_DIR=<ROOT>/debug/tests/session/desktop-session-stage/runtime \
        DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
        DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
        timeout 5s \
        <ROOT>/debug/tests/session/desktop-session-stage/bin/qindaqt-settings
      ```

      exited 3 and reported:

      ```text
      QQmlApplicationEngine failed to load component
      qrc:/qt/qml/QindaQt/SettingsApp/Main.qml:13:1: module "QindaQt.SettingsApp.Power" is not installed
      qrc:/qt/qml/QindaQt/SettingsApp/Main.qml:12:1: module "QindaQt.SettingsApp.Bluetooth" is not installed
      qrc:/qt/qml/QindaQt/SettingsApp/Main.qml:11:1: module "QindaQt.SettingsApp.Audio" is not installed
      ```

   Observed: all three new/extended non-nested rows pass while a required staged
   application cannot load. Expected: the stage-closure row or extended package
   contract fails on this exact pre-`197f104` omission. This defeats one of the
   two real regressions the review brief requires the guard to catch.

### P2

None.

### P3

None.

## Review-question evidence

### Historical Global Menu omission (`e51372a`)

Yes, the new row would have failed on the missing
`libqindaqt_global_menu_qml.so` staging output. The Debug verbose row exited 0
after authenticating 29 ELF files and 333 `DT_NEEDED` entries, then running
its built-in missing-library mutation. I also copied the installed
DesktopVirtual stage under the assigned build root, removed only
`lib64/libqindaqt_global_menu_qml.so`, and invoked the same
`verify_stage_closure` boundary with the row's exact sources, roots, shell
path, and required libraries:

```text
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=tests/session python3 \
  <ROOT>/reproductions/run_closure_negative.py \
  <ROOT>/reproductions/closure-negative.R9oDIz/missing-global-menu \
  libqindaqt_global_menu_qml.so
# exit 0 from the expectation wrapper
EXPECTED FAILURE: .../bin/qindaqt-shell cannot resolve DT_NEEDED libqindaqt_global_menu_qml.so
```

That scratch mutation is the installed-result equivalent of reverting the
`e51372a` DesktopVirtual library staging block, and the row fails for the
intended artifact rather than an unrelated cause.

### Historical Settings route omission (`197f104`)

No. The exact candidate is already the pre-`197f104` state, so no synthetic
source edit is needed: the required Debug and Release rows both passed, while
the isolated offscreen launch above failed on the three missing modules. This
is the blocking finding.

### Applet QML negative control

The shell-applet half of the new row is non-vacuous. I copied the exact closure
stage under the assigned build root, removed only
`QindaQt/Shell/AudioApplet/qmldir`, and ran the verifier with its exact
arguments:

```text
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=tests/session python3 \
  <ROOT>/reproductions/run_closure_negative.py \
  <ROOT>/reproductions/closure-negative.R9oDIz/missing-audio-applet-qmldir \
  'QindaQt.Shell.AudioApplet has no qmldir'
# exit 0 from the expectation wrapper
EXPECTED FAILURE: staged QML module QindaQt.Shell.AudioApplet has no qmldir
```

This proves the source-derived check catches an omitted built-in shell module,
including a module whose implementation target is static. It does not remedy
the absent Settings source inventory.

### Safety and boundaries

- `desktop_session_stage_closure.py:89-100` inspects ELF metadata with
  `readelf`; it does not load staged libraries.
- `test_desktop_session_stage_closure.py:48-65` replaces the environment for
  the shell help probe with a small stage-local/offscreen map, carrying no
  display, Wayland, loader, or session-bus state.
- The candidate changes only the ten documented session-test/wiki paths.
  `DesktopSessionTests.cmake` was decomposed rather than enlarged; the static
  source-shape gate passed.
- No nested compositor, host bus, hardware, uinput, or network row was run. The
  optional nested row was skipped because the review brief states another
  reviewer owns the private seat. `pgrep -af '[k]win_wayland'` happened to
  exit 1 with no process at the instant checked, but that is not authority to
  contend for the assigned lane.

## Commands and results

`<ROOT>` below is
`/home/cabewse/work_SPaC3/builds/qindaqt/review-closure-codex`.

### Identity and cleanliness

```text
git rev-parse HEAD
# exit 0; 99b06199cc256b3a76c1a6c0449690282dd3b2b6

git show -s --format='%H%n%T%n%P' 99b06199cc256b3a76c1a6c0449690282dd3b2b6
# exit 0; candidate/tree/parent match the header

git status --porcelain
# exit 0; empty before review and empty after all review work
```

### Configure and focused builds

```sh
cmake -S . -B <ROOT>/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0

cmake --build <ROOT>/debug --parallel 3 \
  --target qindaqt-desktop-session-probe qindaqt-panel-visibility-session-probe
# exit 0; 1366/1366 Ninja actions

cmake -S . -B <ROOT>/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0

cmake --build <ROOT>/release --parallel 3 \
  --target qindaqt-desktop-session-probe qindaqt-panel-visibility-session-probe
# exit 0; 1366/1366 Ninja actions
```

Both configure runs emitted the repository's existing CMake runtime search-path
warnings but completed successfully.

### Focused and adjacent tests

```text
ctest --test-dir <ROOT>/debug -R 'desktop\.virtual\.(stage-closure|package-contract|sandbox-unit)' --output-on-failure --no-tests=error
# exit 0; 3/3 passed

ctest --test-dir <ROOT>/release -R 'desktop\.virtual\.(stage-closure|package-contract|sandbox-unit)' --output-on-failure --no-tests=error
# exit 0; 3/3 passed

ctest --test-dir <ROOT>/debug -R '^desktop\.virtual\.stage-closure$' -V --no-tests=error
# exit 0; 1/1 passed; 29 ELF files, 333 DT_NEEDED entries,
# five QML modules, missing-library negative control

ctest --test-dir <ROOT>/debug -R '^desktop\.virtual\.sandbox-unit$' -V --no-tests=error
# exit 0; 1/1 CTest row; unittest reported 117/117 passed

ctest --test-dir <ROOT>/debug -R '^session\.python-syntax$' --output-on-failure --no-tests=error
# exit 0; 1/1 passed

ctest --test-dir <ROOT>/release -R '^session\.python-syntax$' --output-on-failure --no-tests=error
# exit 0; 1/1 passed
```

### Static gates

```text
./tools/validate-docs
# exit 0; validated 132 Markdown documents and mkdocs.yml navigation

/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site
# exit 0

./tools/check-source-shape
# exit 0; checked 2208 source files; 0 allowlisted skips;
# five pre-existing decomposition warnings

git diff --check
# exit 0

git diff --check e51372a..99b06199cc256b3a76c1a6c0449690282dd3b2b6
# exit 0
```

No JSON file changed in the candidate, so there was no changed JSON path to pass
to `python3 -m json.tool`.

## Verdict

The candidate correctly closes and guards the shell-linked Global Menu omission
and the five built-in applet qmldirs, and all requested executable/static gates
pass. It does not guard the equally real Settings route staging omission:
the exact candidate reproduces that omission, all new rows pass, and the
staged Settings Center exits before loading its root component. Integration
would therefore preserve a P1 false-negative in the claimed DesktopVirtual
stage-closure contract.

VERDICT REJECT P0/P1/P2/P3=0/1/0/0
