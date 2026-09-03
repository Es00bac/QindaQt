# Ethel Marden handoff — staged shell runtime closure

- Time: 2026-09-02T23:25:56-06:00
- Candidate commit: `b623b005f419722d0ebaea2652cd1046f6adec20`
- Candidate tree: `f95c8ed3714d93511db582ff21667b9e50955ebe`
- Exact base: `24063dd0dda228ee99adfda0bd6ae04cc19cdc4e`
- Accepted Audio ancestor: `caaf7d9a15dd7a936eaee2e8ff83332a5c945970`
- Branch/worktree: `worker/audio-applet-production` at `/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-production`

## Outcome and choice

Implemented option (a), the smallest fix that preserves the accepted runtime
boundary. `qindaqt-shell` has a real ELF dependency on
`libqindaqt_controls_qml.so`; removing that link would reopen the accepted
compiled-QML composition proof. Instead, every narrow Audio, Power, and
Bluetooth component that stages the shell now stages Controls in the shell
libdir and Tokens at Controls' baked `$ORIGIN/../Tokens` sibling destination.
The Power and Bluetooth installed-package rows independently require the
loader to resolve those exact relocated artifacts before launching under
source poison. This changes packaging closure only; no service, controller,
QML, capability, or platform boundary changed.

## Changed paths

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/audio-applet.md`
- `docs/wiki/shell/bluetooth-applet.md`
- `docs/wiki/shell/power-applet.md`
- `src/shell/CMakeLists.txt`
- `tests/shell/bluetooth_applet/run_installed_bluetooth_applet.cmake`
- `tests/shell/power_applet/run_installed_power_applet.cmake`

## Evidence

The assigned Debug and Release roots were already configured with the exact
lane recipe, so they were reused. Both focused builds automatically reran CMake
after the install-graph edit; no separate configure command was run.

Pre-repair negative control:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/audio-applet-production/debug \
  -R '^qindaqt\.(power|bluetooth)-applet-installed-package$' \
  --output-on-failure --no-tests=error
```

- Nonzero as expected, 0/2 passed. Both staged shells failed to load
  `libqindaqt_controls_qml.so` before catalog inspection.

Initial repaired package proof:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/audio-applet-production/debug \
  --parallel 3 --target qindaqt-shell qindaqt_controls_qml qindaqt_tokens_qml
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/audio-applet-production/debug \
  -R '^qindaqt\.(audio|power|bluetooth)-applet-installed-package$' \
  --output-on-failure --no-tests=error
```

- Build exit 0.
- CTest exit 0, 3/3 passed.

Focused build command, run once for each `<profile>` of `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/audio-applet-production/<profile> \
  --parallel 3 --target \
  qindaqt_shell_audio_applet qindaqt_shell_audio_applet_runtime \
  qindaqt_shell_bluetooth_applet qindaqt_shell_bluetooth_applet_runtime \
  qindaqt_shell_power_applet qindaqt_shell_power_applet_runtime \
  qindaqt_audio_applet_model_tests qindaqt_audio_applet_controller_tests \
  qindaqt_audio_applet_qml_tests qindaqt_bluetooth_applet_presentation_tests \
  qindaqt_bluetooth_applet_request_tests qindaqt_bluetooth_applet_controller_tests \
  qindaqt_bluetooth_applet_qml_tests qindaqt_power_applet_presentation_tests \
  qindaqt_power_applet_controls_tests qindaqt_power_applet_request_tests \
  qindaqt_power_applet_controller_tests qindaqt_power_applet_qml_tests \
  qindaqt-shell qindaqt-shell-preview qindaqt_applet_manifest_tests \
  qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests \
  qindaqt_applet_host_policy_tests qindaqt_applet_host_handshake_tests \
  qindaqt_applet_host_lifecycle_tests qindaqt_shell_runtime_options_tests
```

- Debug exit 0; Ninja's final dynamic progress completed 29/29 actions.
- Release exit 0; Ninja's final dynamic progress completed 32/32 actions.
- Both automatic configure passes retained strict warnings and the private
  pinned KWin 6.6.5 cache; existing CMake search-path warnings were nonfatal.

Full applet selector, run in both profiles:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/audio-applet-production/<profile> \
  -R '^qindaqt\.(audio|power|bluetooth)-applet-' \
  --output-on-failure --no-tests=error
```

- Debug exit 0, 21/21 passed.
- Release exit 0, 21/21 passed.

Applet integrity and backend-neutral shell-runtime selector, run in both
profiles:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/audio-applet-production/<profile> \
  -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution|shell-runtime-catalog|shell-runtime-options|notification-center-applet-offscreen|applet-host-policy|applet-host-handshake|applet-host-lifecycle)$' \
  --output-on-failure --no-tests=error
```

- Debug exit 0, 9/9 passed.
- Release exit 0, 9/9 passed.

Static gates:

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/audio-applet-production/site
./tools/check-source-shape
git diff --check
git show --check --oneline --stat b623b005f419722d0ebaea2652cd1046f6adec20
```

- `validate-docs`: exit 0, 117 Markdown documents plus navigation validated.
- strict MkDocs: exit 0, documentation built with no warnings.
- source shape: exit 0, 1,786 files checked and 0 skipped. It reported only
  three pre-existing decomposition-review warnings at 500, 539, and 563
  non-blank lines; no changed file reaches a threshold.
- both whitespace/commit checks: exit 0 with no errors.
- No JSON changed, so no JSON parser command was applicable.

## Bounded caveats

- This candidate proves component packaging, ELF loader closure, compiled QML,
  manifest discovery, source poison, pure/controller/offscreen behavior, and
  backend-neutral shell runtime only.
- It deliberately makes no live PipeWire/WirePlumber, UPower, BlueZ, hardware,
  physical input, host D-Bus, nested compositor, or integrated-session claim.
- No `tests/session` row, host bus/service, network call, hardware path, or
  uinput path was run.

## Requested next action

Independent exact review of
`b623b005f419722d0ebaea2652cd1046f6adec20`, then manager integration.
