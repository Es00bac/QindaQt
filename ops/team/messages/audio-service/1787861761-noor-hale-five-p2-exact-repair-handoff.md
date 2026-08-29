# Audio1 five-P2 exact repair handoff — Noor Hale

Status: handoff-ready for different-worker exact-commit recheck. Do not integrate before that recheck.

## Exact repaired candidate

- Repair commit: `e6423be9040edb5f28dc2f3d8d38665b7ad06030`
- Parent rejected candidate: `6926aad9c93a757d06f32835db9962007ce2b195`
- Original exact base/merge-base: `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- Subject: `Harden Audio1 asynchronous lifecycle boundaries`
- Branch/worktree: `worker/audio1-service` at `/home/cabewse/work_SPaC3/container-wm-workers/audio1-service`
- Clean post-commit product worktree; repair changes 21 files, 1,015 insertions, 128 deletions. This is a new non-amended commit on top of `6926aad`.

## Closed findings and repairs

This commit addresses all five P2s in the review closure `1787860603-codex-audio1-exact-review-reject.md`:

1. `AudioClient` now routes local rejection, unsupported, busy, transport success/failure/status, timeout, owner/epoch replacement, and explicit-stop uncertainty through one receiver-context queued completion mechanism. Tests cover every public operation and all six public result statuses, including a transport that replies synchronously inside submission; no completion is observable before the request-returning method returns, exactly one arrives afterward, repeated stop preserves the stop-created result, cancelled queued results do not leak through stop, and QObject destruction drops queued delivery safely.
2. `AudioBackend` values carry opaque run-generation equality tokens. The production adapter fences on both worker-to-Qt enqueue and Qt delivery; the coordinator rejects stopped/superseded generations before validation/publication. Reusing the resident backend publishes a graph-cleared restarting state and requires the strictly increasing resident service epoch before accepting the new run. Fake and production regressions cover post-stop queues, superseded generations, duplicate/contradictory/revision-regressing snapshots, fresh restart epoch, and outcome fencing.
3. The exact-owner client treats equal epoch+revision only as an identical canonical no-op, rejects changed content and numerical regression under the documented resident-owner monotonic epoch contract, and immediately makes a dispatched mutation `Uncertain/authority-replaced` when it accepts a fresh epoch. A delayed old-epoch success is ignored and never replayed.
4. The coordinator treats every backend outcome as untrusted. Unknown status, empty/non-token/NUL/overlong reason, unsafe/oversized diagnostic, or invalid result lineage atomically becomes protocol-valid `Failed/backend-malformed`; no raw invalid field reaches D-Bus. Adversarial fake-backend cases validate every emitted fallback with the protocol validator.
5. The GLib worker tracks cancellable component loads and `wp_core_sync` callbacks, cancels them during cleanup, and retains the context until every callback releases its owned state before thread join. Component API loading begins only after `wp_core_connect()` accepts the private core. The private unreachable-runtime production regression performs 250 immediate start/stop cycles, bounds descriptor growth to at most five, proves no queued post-stop signal, then proves fresh generation/epoch restart. The previous 501-FD growth and `GWakeup` exhaustion no longer reproduce.

Changed product paths are limited to the existing Audio1 boundary and its primary docs/tests: `src/services/audio_client/**`, `src/services/audio_service/**`, `tests/services/audio_client/**`, `tests/services/audio_service/**`, `docs/wiki/architecture/audio-service.md`, `docs/wiki/reference/audio1-v1.md`, `docs/wiki/adr/0014-confine-wireplumber-to-glib-worker.md`, and `docs/wiki/development/testing-harness.md`. No Settings1/settings app, shell/QML/runtime, themes/profiles, design tokens, applets, Display1/provider, shared availability SDK, PipeWire configuration, or host graph path changed.

## Exact verification evidence

All commands exited 0 on the committed content:

- Debug production build and broad registry: `cmake --build build/audio-dev -j2 && ctest --test-dir build/audio-dev --output-on-failure -j2` — 89/89 passed; Audio1 6/6, isolated runtime 3.18 s, total 16.14 s.
- Release production build and broad registry: `cmake --build build/audio-release -j2 && ctest --test-dir build/audio-release --output-on-failure -j2` — 89/89 passed; Audio1 6/6, isolated runtime 1.85 s, total 22.13 s.
- ASan+UBSan focused registry: targeted build of all six Audio1 tests, then `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/audio-sanitize --output-on-failure -L audio -j1` — 6/6 passed in 3.35 s, including activation and the 250-cycle plus full production runtime test (2.07 s).
- Lifecycle/race repetition: `ctest --test-dir build/audio-dev -R '^qindaqt\.audio-(activation|wireplumber-runtime)$' --repeat until-fail:10 --output-on-failure` — ten consecutive activation passes and ten consecutive isolated-runtime passes. Each runtime pass includes 250 rapid production-backend cycles plus the disposable PipeWire/WirePlumber graph, operations, daemon replacement, and reconnect stress.
- `cmake --install build/audio-release` — staged install updated the public client/backend libraries, headers, and `qindaqt-audio-service` successfully. A manual `dbus-run-session` using only the staged descriptor and private XDG/PipeWire roots activated exact canonical executable PID `3715015`; private daemon death removed that exact PID within the bounded poll. Final exact `/proc` audit found zero candidate service processes, and final fixture-root audit found zero Audio1 temporary roots.
- `./tools/validate-docs` — 43 documents/navigation passed; `uvx --from mkdocs mkdocs build --strict` passed; `./tools/check-source-shape` — 747 files, zero allowlist skips, largest changed handwritten production file `audio_client.cpp` at 476 nonblank lines; `git diff --check` and cached whitespace check passed. No QML/registry changed, so `all_qmllint` is not applicable.

All D-Bus and WirePlumber evidence used private daemons, temporary XDG runtime/config/state roots, a private PipeWire socket, disposable null devices, and synthetic sample generation. No host session bus, host PipeWire/WirePlumber graph, physical microphone/speaker, desktop, input, or system/user configuration was touched.

## Bounded caveats and next ownership

The 250-cycle regression is a deterministic unavailable-private-runtime callback/FD barrier, not a long soak. The full isolated runtime remains functional graph/operation evidence, not a release latency, CPU, PSS/RSS, wakeup, realtime, or sustained-churn budget. The service retains one Qt process plus one GLib worker thread and upstream PipeWire/WirePlumber remains the runtime/policy authority.

USB, HDMI, Bluetooth, jack, multichannel, microphone, hotplug, suspend/resume, hardware gain mapping, real-device behavior, and physical audio quality remain explicitly unqualified. The ratified Settings route `audio`, narrow default-output shell facade plus `openAudioSettings()`, shared availability extraction after two accepted producers, and all accessibility/visual/integrated UI proof remain with future owners; this repair adds no page/applet/UI.

Requested next action: the independent reviewer should recheck exact repaired commit `e6423be9040edb5f28dc2f3d8d38665b7ad06030`, specifically against all five finding records and the committed tests/runtime barrier, then post exact accept/reject evidence. Please review the hash, not this prose, and do not integrate until that different-worker verdict is recorded.

— Noor Hale, Audio1 implementer
