# Audio1 exact-candidate handoff — Noor Hale

Status: handoff-ready for different-worker review.

## Exact candidate

- Commit: `6926aad9c93a757d06f32835db9962007ce2b195`
- Subject: `Add the bounded Audio1 WirePlumber service`
- Branch/worktree: `worker/audio1-service` at `/home/cabewse/work_SPaC3/container-wm-workers/audio1-service`
- Exact base and merge-base: `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- Clean post-commit worktree; 55 files, 5,826 insertions, 2 deletions.

## Outcome and paths

The candidate adds only the assigned Audio1 vertical and small additive registries/docs:

- `src/services/audio_protocol/**`: fixed typed D-Bus structures, bounded decoding, validation, and service constants.
- `src/services/audio_client/**`: exact-unique-owner asynchronous transport/client, epoch/revision fencing, invalidation/refetch, serialized operations, and timeout/owner-replacement uncertainty with no replay.
- `src/services/audio_service/**`: backend abstraction, operation coordinator, resident object/process, activation/systemd/XML packaging, and a libwireplumber 0.5 adapter whose confined GLib worker alone owns `WpCore`, object manager, plugins, and GObjects.
- `tests/services/audio_protocol/**`, `audio_client/**`, `audio_service/**`: roundtrip/bounds/malformed, fake client/backend, private D-Bus, exact activated-process lifecycle, and isolated production WirePlumber runtime proof.
- `docs/wiki/architecture/audio-service.md`, `docs/wiki/reference/audio1-v1.md`, and ADR-0014 plus the additive wiki/build/install registries named in the assignment.

Public handles are `(service epoch, PipeWire object.serial)`. The service exposes bounded real outputs, inputs, application streams, defaults, normalized level/mute state, and typed `SetDefault`, `SetVolume`, `SetMute`, and `MoveStream` operations. WirePlumber/PipeWire replacement advances epoch and makes pending operations uncertain; neither coordinator nor public client replays them. The activated process exits when its exact constructing session bus disconnects. Outstanding `wp_core_sync` callbacks are cancellable and drained on the GLib context before teardown.

No Settings1/settings app, shell QML/runtime, themes/profiles, design tokens, applets, Display1/provider, host/user PipeWire configuration, sample transport, `wpctl` parsing, or shared `ServiceAvailability` SDK was added.

## Evidence

All commands exited 0:

- Debug focused build and registry:
  - `cmake --build build/audio-dev --target qindaqt_audio_protocol_tests qindaqt_audio_client_tests qindaqt_audio_qt_transport_tests qindaqt_audio_activation_tests qindaqt_audio_service_tests qindaqt_audio_wireplumber_runtime_tests -j4`
  - `ctest --test-dir build/audio-dev -R '^qindaqt\.audio-(protocol|client|qt-transport|activation|service|wireplumber-runtime)$' --output-on-failure` — 6/6.
- Debug broad: `ctest --test-dir build/audio-dev --output-on-failure` — 89/89 in 21.65 s.
- Release production build: configure with `-DCMAKE_BUILD_TYPE=Release -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DQINDAQT_BUILD_SHELL=OFF -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF`, then `cmake --build build/audio-release -j4` — 602/602 build steps.
- Release focused — 6/6 in 1.92 s; Release broad — 89/89 in 23.23 s.
- ASan/UBSan: configure with `-fsanitize=address,undefined -fno-omit-frame-pointer`; focused build, then `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest ...` — 6/6 in 2.28 s, including activation and the isolated runtime.
- Race/lifecycle repetition: `ctest --test-dir build/audio-dev -R '^qindaqt\.audio-(activation|wireplumber-runtime)$' --repeat until-fail:10 --output-on-failure` — ten consecutive passes for each. Activation tears down daemon 1, proves the exact canonical-executable PID gone, activates a fresh PID/owner/epoch on daemon 2, and repeats the no-orphan proof. Fixture cleanup revalidates `/proc/<pid>/exe` before bounded TERM-to-KILL fallback and never signals a reused PID.
- Isolated runtime owns a temporary XDG runtime/config/state root, private PipeWire socket, WirePlumber `policy` profile, two null sinks, one null source, and synthetic `pw-cat` stream. It passed graph/default/volume/mute/move, WirePlumber restart/epoch, stale-handle, and eight-operation reconnect/stop lifetime coverage without contacting the host graph or user session bus.
- `cmake --install build/audio-release` — staged install passed. A private `dbus-run-session` found the installed descriptor, activated staged owner `:1.1` at exact PID `3440357`, returned a 187-byte bounded snapshot, and daemon teardown removed that exact process on the first 50 ms poll. Post-test exact `/proc/*/exe` audit found zero candidate Audio1 processes.
- `./tools/validate-docs` — 43 documents and MkDocs navigation pass.
- `uvx --from mkdocs mkdocs build --strict` — pass.
- `./tools/check-source-shape` — 746 files pass, no allowlist skips; largest new handwritten production source is 436 nonblank lines.
- `git diff --cached --check`/post-commit `git status --short` — clean. `all_qmllint` was not applicable because no QML or QML registry changed.

## Bounded caveats and next ownership

The protocol/resource bounds are 128 outputs, 128 inputs, 256 streams, 64 in-flight operations, 256-byte device/application names, and 512-byte safe diagnostics. The staged executable is 260,584 bytes, the systemd unit caps tasks at 16, and the isolated runtime completes in about one second, but that is functional evidence rather than a latency/RSS/long-soak qualification. Release resource budgets remain later work.

USB, HDMI, Bluetooth, jack, multichannel, microphone, hotplug, suspend/resume, hardware gain mapping, and real-device behavior are explicitly unqualified. The ratified Settings route `audio`, narrow default-output shell facade plus `openAudioSettings()`, shared availability extraction after two accepted producers, and all accessibility/visual/integrated UI proof remain with their future owners.

Requested next action: assign a different worker to review exact commit `6926aad9c93a757d06f32835db9962007ce2b195`, including protocol fail-closed bounds, unique-owner/epoch lineage, no-replay uncertainty, constructing-bus death, `OperationSync` teardown, libwireplumber API use, isolated-runtime host guards, packaging hardening, and documentation accuracy. Please recheck the exact repaired hash if any blocker is found; do not approve prose alone.
