# Exact-candidate default-component closure recheck — Audio applet production composition

- Reviewer persona: **Frances Bilas** (`frances-bilas`), independent shell-composition reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `14f3e670d66988ec87648f195b91e26c56a45fbf`
- Candidate tree SHA: `d4bebdd823a54d5cc5522c0e71df4374d3038173`
- Parent SHA: `d47e2e92f4d44df110714c47998d44ae825ac9b3`
- Base SHA: `d47e2e92f4d44df110714c47998d44ae825ac9b3` (exact repair base and parent)
- Rejected product ancestor repaired: `b623b005f419722d0ebaea2652cd1046f6adec20`
- Accepted product ancestor: `caaf7d9a15dd7a936eaee2e8ff83332a5c945970`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-production-glm-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-audioapplet-r2-codex`

The detached worktree matched the exact candidate and was clean before review.
It remained clean after configure, build, tests, manual component installs,
moved-stage execution, and static gates. Product paths were not edited. All
scratch stages and generated documentation remained beneath the assigned build
root.

## Findings ledger

### P0 — none

### P1 — none

### P2 — none

### P3 — none

## Review results

### 1. The default-component regression is closed by construction and execution

The candidate keeps the shell's hard dependency rather than obscuring it:

    readelf -d <stage>/bin/qindaqt-shell
    -> NEEDED libqindaqt_controls_qml.so
    -> RUNPATH $ORIGIN:$ORIGIN/../lib64

    readelf -d <stage>/lib64/libqindaqt_controls_qml.so
    -> NEEDED libqindaqt_tokens_qml.so
    -> RUNPATH $ORIGIN/../Tokens

`src/shell/CMakeLists.txt:181-199` now explicitly assigns the production shell
to component `QindaQt`, installs Controls in `${CMAKE_INSTALL_LIBDIR}`, and
installs Tokens in the sibling `Tokens` directory. The three narrow shell
components retain the same closure at `src/shell/CMakeLists.txt:203-245`,
`:249-290`, and `:295-333`. A repository-wide CMake search found exactly four
install rules carrying target `qindaqt-shell`: default `QindaQt`,
`AudioAppletRuntime`, `BluetoothAppletRuntime`, and `PowerAppletRuntime`.

The new registered row at `tests/shell/audio_applet/CMakeLists.txt:134-149`
passes the shell CMake source into the runner. The runner's inventory check at
`tests/shell/audio_applet/run_shell_component_closure.cmake:20-52` requires all
four explicit component names to match its tested inventory; lines 54-142 then
install each component independently, require all three artifacts, authenticate
the exact resolved Controls and Tokens paths, and execute `--help` with ambient
loader, display, Wayland, and session-bus variables removed. This is not a
vacuous artifact-existence assertion: it resolves both ELF edges and starts
each staged executable.

The exact prior `--component QindaQt` reproduction was rerun in Debug and
Release. Both component-only installs exited 0 and produced:

    <stage>/bin/qindaqt-shell
    <stage>/lib64/libqindaqt_controls_qml.so
    <stage>/Tokens/libqindaqt_tokens_qml.so

With ambient loader variables removed, `ldd` resolved the shell's Controls
edge to `<stage>/bin/../lib64/libqindaqt_controls_qml.so` and its transitive
Tokens edge to `<stage>/bin/../lib64/../Tokens/libqindaqt_tokens_qml.so` in both
profiles. Direct `ldd` of Controls resolved Tokens to
`<stage>/lib64/../Tokens/libqindaqt_tokens_qml.so`. The staged shell's `--help`
command exited 0 in both profiles. This is the same path that exited 127 at
`b623b00`.

All three installed-package rows passed 3/3 separately and again inside the
complete 21-row applet selector in both profiles. The 9-row applet-integrity and
shell-runtime selector also passed in both profiles. Power and Bluetooth
sources, QML, model/controller tests, and every focused test other than the two
installed-package scripts are byte-identical to `caaf7d9`; the exact excluded
diff command exited 0 with no output. The only changed paths in those two module
and test trees remain:

    tests/shell/bluetooth_applet/run_installed_bluetooth_applet.cmake
    tests/shell/power_applet/run_installed_power_applet.cmake

### 2. Relocation and source poison remain executable evidence

After the registered installed-package rows generated fresh stages, I
physically moved all six Debug/Release Audio, Bluetooth, and Power stages to:

    /home/cabewse/work_SPaC3/builds/qindaqt/review-audioapplet-r2-codex/moved-applet-stages-r4.UaQzoJ/

For every moved stage I removed `LD_LIBRARY_PATH`, `DYLD_LIBRARY_PATH`,
`DBUS_SESSION_BUS_ADDRESS`, `DISPLAY`, and `WAYLAND_DISPLAY`; pointed all
ambient/source catalog variables at the moved malformed poison fixtures; passed
only the moved installed profile, theme, applet, and policy paths; and executed
the moved `qindaqt-shell --list`. All six commands exited 0 and found exactly
the expected installed entry (`audio - Audio`, `bluetooth - Bluetooth`, or
`power - Power`). `ldd` in the same moved locations resolved Controls and
Tokens inside each exact moved stage, with no `not found` result.

The registered scripts independently create malformed source-path fixtures and
perform the same explicit installed-data override. Their 3/3 Debug and Release
passes therefore retain the source-poison proof as well as the manual physical
move evidence.

### 3. Documentation and static policy are truthful

The Audio, Power, Bluetooth, applet-runtime, and testing-harness pages describe
the four-component inventory, exact relative loader layout, isolated component
row, and bounded no-host/no-hardware claims accurately. They do not claim live
services, compositor, hardware, or nested-session coverage. The candidate adds
no service, controller, QML, persistence, platform, or runtime authority.

The repair is additive to the install graph and test registry. Documentation,
strict MkDocs, source-shape, and whitespace gates all passed. No JSON changed
between `b623b00` and the candidate, so no changed-JSON parser invocation was
applicable.

## Commands and executed results

`<ROOT>` below is
`/home/cabewse/work_SPaC3/builds/qindaqt/review-audioapplet-r2-codex`.

### Identity and cleanliness

    git rev-parse HEAD
    -> 14f3e670d66988ec87648f195b91e26c56a45fbf

    git rev-parse HEAD^{tree}
    -> d4bebdd823a54d5cc5522c0e71df4374d3038173

    git rev-parse HEAD^
    -> d47e2e92f4d44df110714c47998d44ae825ac9b3

    git merge-base caaf7d9 14f3e67
    -> caaf7d9a15dd7a936eaee2e8ff83332a5c945970

    git merge-base b623b00 14f3e67
    -> b623b005f419722d0ebaea2652cd1046f6adec20

    git status --porcelain=v1
    git status --porcelain=v1 --ignored -- src/shell tests/shell docs/wiki
    -> empty before and after review

### Configure

Both prescribed commands exited 0 and generated successfully. CMake emitted
the existing dependency-prefix versus system-library safe-RPATH warnings.

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

This exact command was run for `<profile>` equal to `debug` and `release`;
both incremental builds exited 0:

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

Every command below used `--output-on-failure --no-tests=error` and exited 0:

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

    ctest --test-dir <ROOT>/<profile> \
      -R '^qindaqt\.shell-runtime-component-closure$' \
      --output-on-failure --no-tests=error
    -> Debug 1/1; Release 1/1

The component-closure row independently installed and launched all four
shell-carrying components in each profile.

### Exact default-component reproduction

The exact component-only install was run in both profiles:

    cmake --install <ROOT>/<profile>/src/shell \
      --prefix <ROOT>/qindaqt-default-component-r4.TEWhRt/<profile> \
      --component QindaQt
    -> Debug exit 0; Release exit 0
    -> shell, Controls, and Tokens present in both stages

    test -x <stage>/bin/qindaqt-shell
    test -e <stage>/lib64/libqindaqt_controls_qml.so
    test -e <stage>/Tokens/libqindaqt_tokens_qml.so
    env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH \
      ldd <stage>/bin/qindaqt-shell
    env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH \
      ldd <stage>/lib64/libqindaqt_controls_qml.so
    env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH \
        -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
        <stage>/bin/qindaqt-shell --help
    -> Debug exit 0; Release exit 0
    -> exact in-stage Controls and Tokens paths in both profiles

    readelf -d <stage>/bin/qindaqt-shell
    readelf -d <stage>/lib64/libqindaqt_controls_qml.so
    -> Debug exit 0; Release exit 0
    -> hard Controls/Tokens NEEDED edges and expected relative RUNPATHs

### Moved-stage source-poison reproduction

For each of the six moved profile/applet stages, the executed command shape was:

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
    -> 6/6 exited 0 and found the expected installed applet

    env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH \
      ldd <moved-stage>/bin/qindaqt-shell
    -> 6/6 resolved Controls and Tokens inside the exact moved stage

### Diff and static gates

    git diff --exit-code caaf7d9..14f3e67 -- \
      src/shell/power_applet src/shell/bluetooth_applet \
      tests/shell/power_applet tests/shell/bluetooth_applet \
      ':(exclude)tests/shell/power_applet/run_installed_power_applet.cmake' \
      ':(exclude)tests/shell/bluetooth_applet/run_installed_bluetooth_applet.cmake'
    -> exit 0; no output

    ./tools/validate-docs
    -> exit 0; 117 Markdown documents and mkdocs.yml navigation validated

    /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build \
      --strict --site-dir <ROOT>/site
    -> exit 0; documentation built without warnings

    ./tools/check-source-shape
    -> exit 0; 1,787 source files checked, 0 skipped
    -> only the three existing decomposition-review warnings at 500, 539, and
       563 non-blank lines; the new runner is below the threshold

    git diff --check
    git diff --check b623b00..14f3e67
    git show --check --oneline --stat 14f3e67
    -> all exit 0

    git diff --name-only b623b00..14f3e67 -- '*.json'
    -> no output; changed-JSON parser gate not applicable

### Intentionally not run

Per the brief, no `tests/session` nested-compositor row, host D-Bus
system/session service, hardware, uinput, or network test was run. The selected
rows use pure/injected/offscreen behavior and disposable installed stages only.

## Verdict

The candidate closes the rejected default `QindaQt` component by construction,
with direct Debug and Release process-load evidence, while preserving the three
applet component closures, moved-stage/source-poison proof, accepted applet
implementation, and truthful documentation. No blocking or nonblocking defect
was found in this bounded repair.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
