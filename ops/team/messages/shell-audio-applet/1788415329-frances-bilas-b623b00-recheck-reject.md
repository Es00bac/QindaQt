# Exact-candidate staging-closure recheck — Audio applet production composition

- Reviewer persona: **Frances Bilas** (`frances-bilas`), independent shell-composition reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `b623b005f419722d0ebaea2652cd1046f6adec20`
- Candidate tree SHA: `f95c8ed3714d93511db582ff21667b9e50955ebe`
- Parent SHA: `24063dd0dda228ee99adfda0bd6ae04cc19cdc4e`
- Base SHA: `24063dd0dda228ee99adfda0bd6ae04cc19cdc4e` (candidate's exact repair base and parent)
- Accepted product ancestor: `caaf7d9a15dd7a936eaee2e8ff83332a5c945970`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-production-glm-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-audioapplet-r2-codex`

The worktree was detached at the exact candidate and clean before review. It
remained clean after configure, build, tests, relocated-stage exercises, and
static gates. Product paths were not edited. Scratch stages remained beneath
the assigned build root.

## Findings ledger

### P0 — none

### P1 — 1

**P1-1: the default `QindaQt` install component still stages a production
shell without its hard Controls/Tokens runtime closure.**

`src/shell/CMakeLists.txt:181` installs `qindaqt-shell` without an explicit
component. In this configured project, CMake assigns that rule to the default
component named `QindaQt` (the generated
`<ROOT>/<profile>/src/shell/cmake_install.cmake:65-83` condition is
`CMAKE_INSTALL_COMPONENT STREQUAL "QindaQt"`). The candidate adds Controls and
Tokens only to the three applet components at
`src/shell/CMakeLists.txt:218-227`, `:264-273`, and `:306-315`. The ordinary
Controls/Tokens QML installs belong to `SettingsAppearanceRuntime`, not
`QindaQt` (`src/controls/CMakeLists.txt:88-97` and
`src/design_tokens/CMakeLists.txt:71-81`).

Exact focused reproduction, run in both profiles:

    cmake --install <ROOT>/<profile>/src/shell \
      --prefix <ROOT>/qindaqt-subdir-component-proof.DmlINR/<profile> \
      --component QindaQt

    test -x <stage>/bin/qindaqt-shell
    test -e <stage>/lib64/libqindaqt_controls_qml.so
    test -e <stage>/Tokens/libqindaqt_tokens_qml.so
    env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH \
        -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
        <stage>/bin/qindaqt-shell --help

Observed in Debug and Release: install exit 0; shell present; Controls missing;
Tokens missing; `ldd` reports `libqindaqt_controls_qml.so => not found`; shell
exits 127 with `error while loading shared libraries:
libqindaqt_controls_qml.so`. Expected: every component that carries the
production shell either stages its hard Controls/Tokens closure or does not
carry the shell. The bounded workaround is to install the full graph or one of
the three applet components, but the named default component itself is broken.

The three requested applet package rows cannot detect this because they install
only `AudioAppletRuntime`, `PowerAppletRuntime`, and
`BluetoothAppletRuntime`. Bounded repair: add the same Controls/Tokens layout
to component `QindaQt` (or move the componentless shell rule to an explicitly
closed runtime component) and add a component-focused installed negative row.

### P2 — none

### P3 — none

## Review-question results

### 1. Staging regression closure — applet components closed; default component open

`src/shell/CMakeLists.txt:185-227`, `:231-273`, and `:277-315` enumerate the
three named applet install components that carry `qindaqt-shell`:
`BluetoothAppletRuntime`, `AudioAppletRuntime`, and `PowerAppletRuntime`.
Each component now also installs:

- `qindaqt_controls_qml` in `${CMAKE_INSTALL_LIBDIR}`, beside the shell's
  `$ORIGIN/../lib64` lookup; and
- `qindaqt_tokens_qml` in `Tokens`, matching Controls' baked
  `$ORIGIN/../Tokens` lookup.

The candidate missed the componentless shell rule at line 181, which CMake
places in the named default `QindaQt` component. A full unfiltered install does
execute all applet-component rules and therefore gains the closure, but a
filtered `--component QindaQt` install executes the shell rule without any of
the candidate's closure rules. P1-1 is the resulting exact failure.

The built ELF evidence is direct:

    readelf -d <moved-stage>/bin/qindaqt-shell
    -> NEEDED libqindaqt_controls_qml.so
    -> RUNPATH $ORIGIN:$ORIGIN/../lib64

    readelf -d <moved-stage>/lib64/libqindaqt_controls_qml.so
    -> NEEDED libqindaqt_tokens_qml.so
    -> RUNPATH $ORIGIN/../Tokens

Thus the hard link was not removed. All six independently moved Debug/Release
stages resolved the libraries as:

    libqindaqt_controls_qml.so => <moved-stage>/bin/../lib64/libqindaqt_controls_qml.so
    libqindaqt_tokens_qml.so   => <moved-stage>/bin/../lib64/../Tokens/libqindaqt_tokens_qml.so

The three installed-package rows passed separately 3/3 and within the complete
21-row applet selector in both profiles. This closes the observed Power and
Bluetooth applet-component loader failures, but does not close the separate
default `QindaQt` component demonstrated in P1-1.

The Power and Bluetooth implementation, QML, controller/model tests, and all
other focused tests are byte-identical to `caaf7d9`. The exact proof was:

    git diff --exit-code caaf7d9..b623b00 -- \
      src/shell/power_applet src/shell/bluetooth_applet \
      tests/shell/power_applet tests/shell/bluetooth_applet \
      ':(exclude)tests/shell/power_applet/run_installed_power_applet.cmake' \
      ':(exclude)tests/shell/bluetooth_applet/run_installed_bluetooth_applet.cmake'
    -> exit 0, no output

The only changed paths in those four trees are the two named installed-package
scripts.

### 2. Relocation and source poison — retained

After the registered package rows created their stages, I physically moved all
three stages from each profile into:

    /home/cabewse/work_SPaC3/builds/qindaqt/review-audioapplet-r2-codex/relocation-proof.oZuBgM/

For each of `debug-audio`, `debug-bluetooth`, `debug-power`, `release-audio`,
`release-bluetooth`, and `release-power`, I cleared ambient library, display,
Wayland, and session-bus variables, pointed all source-derived catalog
environment paths at malformed poison fixtures inside the moved stage, and ran
the moved executable with only the moved installed data paths. All six commands
exited 0 and returned the expected installed entry (`audio - Audio`,
`bluetooth - Bluetooth`, or `power - Power`). `ldd` under the same cleared
library environment produced the exact moved-stage Controls/Tokens resolutions
quoted above.

The registered scripts also clear ambient loader paths and execute `--list`
against malformed source-path fixtures. Bluetooth authenticates exact KF6,
Controls, and Tokens real paths; Power authenticates that Controls remains
inside the relocated stage and Tokens is at the exact sibling path; Audio
authenticates exact KF6 and Tokens paths and then necessarily loads the staged
Controls library during the poisoned launch.

### 3. Documentation, source shape, and additive scope — focused claims pass

The three applet pages and testing-harness page accurately describe the three
applet components and their tests. They do not claim that the default
`QindaQt` component is closed, so P1-1 is a product/install-graph failure
rather than an additional documentation overclaim. No service, controller,
QML, platform, host-bus, hardware, or nested-session claim was added.

The `caaf7d9..b623b00` product change in `src/shell/CMakeLists.txt` is additive
install composition. The two test-script changes add dependency authentication
without weakening the existing compiled-QML, relocation, manifest, or source-
poison checks. Static documentation, source-shape, and whitespace gates all
passed. No JSON file changed, so the changed-JSON parser gate was not
applicable.

## Commands and executed results

### Identity and cleanliness

    git rev-parse HEAD
    -> b623b005f419722d0ebaea2652cd1046f6adec20

    git rev-parse HEAD^{tree}
    -> f95c8ed3714d93511db582ff21667b9e50955ebe

    git rev-parse HEAD^
    -> 24063dd0dda228ee99adfda0bd6ae04cc19cdc4e

    git merge-base caaf7d9 b623b00
    -> caaf7d9a15dd7a936eaee2e8ff83332a5c945970

    git status --porcelain=v1
    git status --porcelain=v1 --ignored -- src/shell tests/shell docs/wiki
    -> empty before and after review

### Configure

Both exact commands exited 0. CMake emitted the existing dependency-prefix vs
system-library safe-RPATH warnings and generated both build trees successfully.

    cmake -S . -B <ROOT>/debug -G Ninja \
      -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
      -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
      -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
      -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
      -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
      -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

    cmake -S . -B <ROOT>/release -G Ninja \
      -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
      -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
      -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
      -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
      -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

### Focused builds

The following command was executed once for each `<profile>` of `debug` and
`release`; both exited 0, with Ninja's final dynamic progress at 31/31:

    cmake --build <ROOT>/<profile> --parallel 3 --target \
      qindaqt_shell_audio_applet qindaqt_shell_audio_applet_runtime \
      qindaqt_shell_bluetooth_applet qindaqt_shell_bluetooth_applet_runtime \
      qindaqt_shell_power_applet qindaqt_shell_power_applet_runtime \
      qindaqt_audio_applet_model_tests qindaqt_audio_applet_controller_tests \
      qindaqt_audio_applet_qml_tests \
      qindaqt_bluetooth_applet_presentation_tests \
      qindaqt_bluetooth_applet_request_tests \
      qindaqt_bluetooth_applet_controller_tests \
      qindaqt_bluetooth_applet_qml_tests \
      qindaqt_power_applet_presentation_tests \
      qindaqt_power_applet_controls_tests qindaqt_power_applet_request_tests \
      qindaqt_power_applet_controller_tests qindaqt_power_applet_qml_tests \
      qindaqt-shell qindaqt-shell-preview qindaqt_applet_manifest_tests \
      qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests \
      qindaqt_applet_host_policy_tests qindaqt_applet_host_handshake_tests \
      qindaqt_applet_host_lifecycle_tests qindaqt_shell_runtime_options_tests

### CTest selectors

Each command used `--output-on-failure --no-tests=error` and exited 0:

    ctest --test-dir <ROOT>/<profile> \
      -R '^qindaqt\.(audio|power|bluetooth)-applet-installed-package$' \
      --output-on-failure --no-tests=error
    -> Debug 3/3; Release 3/3

    ctest --test-dir <ROOT>/<profile> \
      -R '^qindaqt\.(audio|power|bluetooth)-applet-' \
      --output-on-failure --no-tests=error
    -> Debug 21/21; Release 21/21

    ctest --test-dir <ROOT>/<profile> \
      -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution|shell-runtime-catalog|shell-runtime-options|notification-center-applet-offscreen|applet-host-policy|applet-host-handshake|applet-host-lifecycle)$' \
      --output-on-failure --no-tests=error
    -> Debug 9/9; Release 9/9

### Independent moved-stage command and results

The executed loop moved each existing registered-test stage, then ran this
command shape for all six profile/applet combinations:

    env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH \
        -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
        XDG_RUNTIME_DIR=<moved-stage>/source-poison/runtime \
        XDG_DATA_DIRS=<moved-stage>/source-poison \
        QINDAQT_PROFILE_DIR=<moved-stage>/source-poison/profiles \
        QINDAQT_THEME_DIR=<moved-stage>/source-poison/themes \
        QINDAQT_APPLET_DIR=<moved-stage>/source-poison/applets \
        QINDAQT_APPLET_POLICY=<moved-stage>/source-poison/policy.json \
        <moved-stage>/bin/qindaqt-shell --list \
        --profile-dir=<moved-stage>/share/qindaqt/profiles \
        --theme-dir=<moved-stage>/share/qindaqt/themes \
        --applet-dir=<moved-stage>/share/qindaqt/applets \
        --applet-policy=<moved-stage>/share/qindaqt/applet-policy/default.json
    -> six of six exit 0; each expected installed applet discovered

    env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH \
      ldd <moved-stage>/bin/qindaqt-shell
    -> six of six resolve Controls and Tokens inside that exact moved stage

### Default-component negative reproduction

The shell-owned install subtree was used so the exact candidate's focused
build did not need unrelated `QindaQt` component targets from other modules:

    cmake --install <ROOT>/<profile>/src/shell \
      --prefix <ROOT>/qindaqt-subdir-component-proof.DmlINR/<profile> \
      --component QindaQt
    -> Debug exit 0; Release exit 0
    -> qindaqt-shell present in both stages
    -> lib64/libqindaqt_controls_qml.so missing in both stages
    -> Tokens/libqindaqt_tokens_qml.so missing in both stages

    env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH \
      ldd <stage>/bin/qindaqt-shell
    -> Debug and Release: libqindaqt_controls_qml.so => not found

    env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH \
        -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
        <stage>/bin/qindaqt-shell --help
    -> Debug and Release exit 127: libqindaqt_controls_qml.so cannot be opened

### Static gates

    ./tools/validate-docs
    -> exit 0; 117 Markdown documents and mkdocs.yml navigation validated

    /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build \
      --strict --site-dir <ROOT>/site
    -> exit 0; documentation built without warnings

    ./tools/check-source-shape
    -> exit 0; 1,786 source files checked, 0 skipped
    -> only the three pre-existing decomposition-review warnings at 500, 539,
       and 563 non-blank lines; no changed file reaches a threshold

    git diff --check
    git diff --check caaf7d9..b623b00
    git show --check --oneline --stat b623b00
    -> all exit 0; no whitespace error

    git diff --name-only caaf7d9..b623b00 -- '*.json'
    -> no output; no changed JSON to parse

### Intentionally not run

Per the brief, no `tests/session` nested-compositor row, host D-Bus
system/session service, hardware, uinput, or network test was run. The selected
rows use pure/injected/offscreen behavior and disposable installed stages only.

## Verdict

The candidate closes all three applet-component stages without weakening the
accepted Audio composition, and every requested selector/static gate passes.
However, the separate default `QindaQt` component still installs the same hard-
linked production shell without Controls or Tokens and fails at process load in
both profiles. Because the review explicitly requires every shell-carrying
component to be closed, P1-1 blocks integration.

VERDICT REJECT P0/P1/P2/P3=0/1/0/0
