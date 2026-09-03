# Handoff — QQ-004.12 Audio applet production composition

- Worker: Mary Allen Wilkes (`mary-allen-wilkes`)
- Candidate commit: `807b1aeed22de3852b18c1609bd9eae04b2b67d9`
- Candidate tree: `5e9e5816a17e509e78db5ac093199aeacaec99e9`
- Exact base: `35f2fa20881437fc3ef9d85ce399dc68e12ed1d3` (Bluetooth B1 tip; parent of candidate)
- Branch: `worker/audio-applet-production`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-production`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/audio-applet-production`

## Outcome

The bounded Audio applet (`src/shell/audio_applet`) is now a production-hosted
applet, mirroring the Power P2 and Bluetooth B1 composition: manifest with
separate least-authority `audio.read` / `audio.control` grants, policy
registration, built-in registry entry `qindaqt.applets.audio`, stock profile
placement, shell-private `AudioAppletComposition` over the public `AudioClient`
seam, shell/preview QML hosting with Power-parity keyboard and accessibility,
and six focused `qindaqt.audio-applet-*` test rows.

## Changed paths (sorted)

- `data/applets/audio.json`
- `data/profiles/qindaqt.json`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/applet-manifest-schema-v1.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/audio-applet.md`
- `src/applet_runtime/src/builtin_applet_registry.cpp`
- `src/shell/audio_applet/CMakeLists.txt`
- `src/shell/audio_applet/audio_applet_controller.cpp`
- `src/shell/audio_applet/audio_applet_controller.h`
- `src/shell/audio_applet/audio_applet_model.cpp`
- `src/shell/audio_applet/audio_applet_model.h`
- `src/shell/audio_applet/qml/AudioDeviceRow.qml`
- `src/shell/audio_applet/qml/AudioStreamRow.qml`
- `src/shell/CMakeLists.txt`
- `src/shell/qml/AppletChip.qml`
- `src/shell/qml/BuiltinAppletContent.qml`
- `src/shell/qml/PanelAppletColumn.qml`
- `src/shell/qml/PanelAppletRow.qml`
- `src/shell/qml/PanelContent.qml`
- `src/shell/qml/RuntimePanel.qml`
- `src/shell/runtime/audioappletcomposition.cpp`
- `src/shell/runtime/audioappletcomposition.h`
- `src/shell/runtime/runtimepanelwindowfactory.cpp`
- `src/shell/runtime/runtimepanelwindowfactory.h`
- `src/shell/runtime/shellruntimeapplication.cpp`
- `src/shell/runtime/shellruntimeapplication.h`
- `tests/applet_runtime/tst_applet_instance_resolver.cpp`
- `tests/applets/tst_catalog.cpp`
- `tests/applets/tst_manifest.cpp`
- `tests/shell/audio_applet/CMakeLists.txt`
- `tests/shell/audio_applet/check_boundary.cmake`
- `tests/shell/audio_applet/check_runtime_boundary.cmake`
- `tests/shell/audio_applet/run_installed_audio_applet.cmake`
- `tests/shell/audio_applet/tst_audio_applet_controller.cpp`
- `tests/shell/audio_applet/tst_audio_applet_qml.cpp`

## Evidence

Configure (Debug and Release, both exit 0):

    cmake -S . -B <ROOT>/<profile> -G Ninja \
      -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
      -DCMAKE_BUILD_TYPE=<Debug|Release> -DBUILD_TESTING=ON \
      -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
      -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
      -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

Focused build (strict warnings-as-errors), Debug exit 0:

    cmake --build <ROOT>/debug --parallel 3 --target \
      qindaqt_shell_audio_applet qindaqt_shell_audio_applet_runtime \
      qindaqt_audio_applet_model_tests qindaqt_audio_applet_controller_tests \
      qindaqt_audio_applet_qml_tests qindaqt-shell qindaqt-shell-preview \
      qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests \
      qindaqt_applet_instance_resolver_tests

Same target list built in Release, exit 0. Adjacent test executables
(`qindaqt_applet_host_{policy,handshake,lifecycle}_tests`,
`qindaqt_shell_runtime_options_tests`,
`qindaqt_{power,bluetooth}_applet_{qml,controller}_tests`) built in both
profiles, exit 0.

Debug tests, exit 0:

- `ctest -R '^qindaqt\.audio-applet-'` — 6/6 passed (model, controller,
  offscreen, boundary, runtime-boundary, installed-package)
- `ctest -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution)$'` — 3/3
- `ctest -R '^qindaqt\.(shell-runtime-catalog|shell-runtime-options|notification-center-applet-offscreen|applet-host-policy|applet-host-handshake|applet-host-lifecycle)$'` — 6/6
- `ctest -R '^qindaqt\.(power|bluetooth)-applet-(offscreen|controller)$'` — 4/4

Release tests, exit 0:

- `ctest -R '^qindaqt\.audio-applet-'` — 6/6 passed
- `ctest -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution)$'` — 3/3
- combined shell/applet-host/Power/Bluetooth selector (10 rows) — 10/10

Static gates, all exit 0 (from worktree root):

- `./tools/validate-docs` (117 documents)
- `mkdocs build --strict --site-dir <ROOT>/site`
- `./tools/check-source-shape`
- `git diff --check`
- `python3 -m json.tool` on `data/applets/audio.json` and `data/profiles/qindaqt.json`

No nested-compositor, host D-Bus, hardware, uinput, or network rows were run;
all tests use private buses and injected fakes (the existing fake audio
transport and a staged install tree).

## Bounded caveats

- The Audio applet intentionally has no summary/popup chrome: it renders the
  bounded device/stream list directly in the chip, documented on the wiki page.
- Volume rows dispatch on every `Slider.moved` (Power parity). Qt 6.11 reports
  `pressed=true` during keyboard steps, so press-state gating is impossible;
  the row pending state disables the slider and the controller refuses
  overlapping requests.
- The installed-package row stages `libqindaqt_controls_qml.so` via the
  component and copies `libqindaqt_tokens_qml.so` to the baked `$ORIGIN/../Tokens`
  sibling path inside the test, authenticating the loader path with
  `file(GET_RUNTIME_DEPENDENCIES)`.
- Power/Bluetooth paths were left byte-identical; their rows were rerun only as
  regression evidence in this tree.

## Requested next action

Independent exact review of candidate `807b1aeed22de3852b18c1609bd9eae04b2b67d9`,
then manager integration.
