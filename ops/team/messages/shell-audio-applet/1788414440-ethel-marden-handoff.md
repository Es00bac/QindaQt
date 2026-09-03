# Default-component shell closure handoff

- Implementer: Ethel Marden (`ethel-marden`), shell composition repair implementer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate commit: `14f3e670d66988ec87648f195b91e26c56a45fbf`
- Candidate tree: `d4bebdd823a54d5cc5522c0e71df4374d3038173`
- Exact repair base: `d47e2e92f4d44df110714c47998d44ae825ac9b3`
- Rejected product ancestor repaired: `b623b005f419722d0ebaea2652cd1046f6adec20`
- Branch: `worker/audio-applet-production`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-production`
- Timestamp: `2026-09-02T23:47:20-06:00`

## Outcome and bounded choice

The candidate closes the single P1 from Frances Bilas's exact recheck. I chose
the smallest install-graph repair: retain the shell's hard Controls dependency,
assign its formerly componentless install explicitly to default component
`QindaQt`, and install `qindaqt_controls_qml` plus `qindaqt_tokens_qml` into
that same relative `lib64`/`Tokens` layout. Existing Audio, Bluetooth, and Power
component payloads are unchanged.

The new `qindaqt.shell-runtime-component-closure` row installs all four
shell-carrying components separately from the shell install subtree,
authenticates exact in-stage Controls and Tokens resolutions, and executes each
staged shell with `LD_LIBRARY_PATH`, `DYLD_LIBRARY_PATH`,
`DBUS_SESSION_BUS_ADDRESS`, `DISPLAY`, and `WAYLAND_DISPLAY` unset. It also
compares its test inventory with every explicit `qindaqt-shell` install rule,
so a newly added component cannot silently avoid the isolation matrix.

## Changed paths

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/audio-applet.md`
- `docs/wiki/shell/bluetooth-applet.md`
- `docs/wiki/shell/power-applet.md`
- `src/shell/CMakeLists.txt`
- `tests/shell/audio_applet/CMakeLists.txt`
- `tests/shell/audio_applet/run_shell_component_closure.cmake`

## Executed evidence

All build and test output remained beneath
`/home/cabewse/work_SPaC3/builds/qindaqt/audio-applet-production`; no host
desktop, host D-Bus service, hardware, uinput, network, or nested-compositor row
was touched.

Negative control before regenerating the repaired install graph:

```sh
cmake \
  -DQINDAQT_CMAKE=$(command -v cmake) \
  -DQINDAQT_SHELL_BUILD_ROOT=<ROOT>/debug/src/shell \
  -DQINDAQT_STAGE_ROOT=<ROOT>/debug/tests/shell/audio_applet/negative-control-shell-components \
  -DQINDAQT_INSTALL_BINDIR=bin -DQINDAQT_INSTALL_LIBDIR=lib64 \
  -P tests/shell/audio_applet/run_shell_component_closure.cmake
```

Expected exit 1: the unrepaired `QindaQt` stage was missing
`lib64/libqindaqt_controls_qml.so`. This demonstrates the new row fails on the
rejected graph.

Both prescribed configure commands were run for Debug and Release and exited
0. They were rerun after finalizing test registration; both final reruns also
exited 0:

```sh
cmake -S . -B <ROOT>/<profile> -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=<Debug-or-Release> -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

The dependency-prefix versus system-library safe-RPATH warnings were the
existing configure warnings; generation completed successfully.

The following focused build command ran once in each profile and exited 0:

```sh
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
```

Final CTest results, each with `--output-on-failure --no-tests=error`:

```sh
ctest --test-dir <ROOT>/<profile> \
  -R '^qindaqt\.(audio|power|bluetooth)-applet-' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 21/21 passed.
- Release: exit 0, 21/21 passed.
- The selector was run twice as the test runner was finalized; all four
  executions passed their full 21/21 set.

```sh
ctest --test-dir <ROOT>/<profile> \
  -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution|shell-runtime-catalog|shell-runtime-options|notification-center-applet-offscreen|applet-host-policy|applet-host-handshake|applet-host-lifecycle)$' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 9/9 passed.
- Release: exit 0, 9/9 passed.
- The selector was run twice as the test runner was finalized; all four
  executions passed their full 9/9 set.

```sh
ctest --test-dir <ROOT>/<profile> \
  -R '^qindaqt\.shell-runtime-component-closure$' \
  --output-on-failure --no-tests=error
```

- Debug final: exit 0, 1/1 passed.
- Release final: exit 0, 1/1 passed.
- An earlier runtime-only version passed 1/1 in each profile. After the
  inventory parser was added, one intermediate execution in each profile
  failed because its target-name regex also matched `qindaqt-shell-preview`;
  requiring whitespace after the exact `qindaqt-shell` target repaired that
  test defect, and the final executions above passed.

Static gates were run after the final test-script repair:

```sh
./tools/validate-docs
```

Exit 0; 117 Markdown documents and `mkdocs.yml` navigation validated.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build \
  --strict --site-dir <ROOT>/site
```

Exit 0; documentation built without warnings.

```sh
./tools/check-source-shape
```

Exit 0; 1,787 files checked and 0 skipped. It reported only the existing
decomposition-review warnings at 500, 539, and 563 non-blank lines; the new
runner is 145 lines.

```sh
git diff --check
git diff --cached --check
git show --check --oneline --stat 14f3e670d66988ec87648f195b91e26c56a45fbf
```

All exited 0. The first static gate set, run before the final regex tightening,
also exited 0. No JSON changed, so no JSON parser command was applicable.

## Bounded caveats

- This is component-filtered install, relative loader-path, and process-start
  evidence only. It makes no live compositor, Audio1/Power1/Bluetooth1 service,
  PipeWire/WirePlumber, BlueZ, power-daemon, physical-hardware, realtime,
  accessibility-bridge, or nested-session claim.
- The isolated default `QindaQt` proof uses the shell-owned install subtree so
  it does not require building unrelated default-component targets elsewhere
  in the repository. It still exercises the exact generated shell install
  rules Frances used in her reproduction.
- External platform libraries remain packaging authorities outside this P1;
  this candidate closes the two QindaQt-owned hard runtime libraries identified
  by the verdict.

## Requested next action

Frances Bilas: independently recheck exact candidate
`14f3e670d66988ec87648f195b91e26c56a45fbf`, including the default `QindaQt`
reproduction and all four component-isolation cases, then return the exact
verdict for Program Manager integration.

Requested disposition: **independent exact review by Frances Bilas, then
manager integration**.
