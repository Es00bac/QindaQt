# Audio1 run-scoped reset-source exact repair handoff — Noor Hale

Status: handoff-ready for the same independent reviewer's exact-commit recheck. Do not integrate before that verdict.

## Exact repaired candidate

- Follow-up commit: `bd3a94e32aff5a5bd8bde737aae62e8330241734`
- Exact tree: `f7d01c8b54aba090be7a21ebaf98f782d3348bea`
- Parent: `e6423be9040edb5f28dc2f3d8d38665b7ad06030`
- Original exact base/merge-base: `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- Subject: `Make Audio1 disconnect resets run-scoped`
- Branch/worktree: `worker/audio1-service` at `/home/cabewse/work_SPaC3/container-wm-workers/audio1-service`
- Clean post-commit product worktree. This is a new, non-amended 7-file follow-up on `e6423be`: 432 insertions and 18 deletions.

Changed paths:

- `src/services/audio_service/src/wireplumber_worker.cpp`
- `src/services/audio_service/src/wireplumber_worker_p.h`
- `tests/services/audio_service/CMakeLists.txt`
- `tests/services/audio_service/tst_wireplumber_reset_lifecycle.cpp`
- `docs/wiki/adr/0014-confine-wireplumber-to-glib-worker.md`
- `docs/wiki/architecture/audio-service.md`
- `docs/wiki/development/testing-harness.md`

## Finding repair

This commit addresses `1787862747-codex-audio1-reset-latch-restart-finding.md` without relying on scheduler timing:

- The process-lifetime `m_resetScheduled` boolean is gone. The GLib worker now explicitly owns the scheduled `GSource *`, and the source payload carries the exact worker-run token.
- Cleanup/stop synchronously destroys and releases any owned disconnect-reset source before core teardown. Normal dispatch releases that ownership exactly once, and both paths are safe if the other wins first.
- Every GLib worker run receives a new equality token. Stale prior-run reset work is rejected before it can tear down the new core, advance epoch, reconnect, or publish.
- `AGENT-GUARD` comments and ADR/architecture/testing text record the source-ownership, run-token, and stop-barrier contract for future changes.
- New `qindaqt.audio-wireplumber-reset-lifecycle` deterministically queues the real disconnect idle, pauses that worker turn, queues the real higher-priority stop from another thread, releases the turn, and requires cleanup to cancel the idle. It then restarts on a fresh private PipeWire runtime and proves the next real loss is not suppressed. The scenario performs two complete loss/stop/restart/loss cycles, requires each second loss to publish `pipewire-unavailable`, requires the resident epoch to advance exactly once, and bounds descriptor growth to at most five.

## Exact verification evidence

All commands exited 0 on the committed content; compilation was sequenced and capped at parallel 2:

- Debug focused Audio1: `ctest --test-dir build/audio-dev --output-on-failure -L audio -j2` — 7/7 passed in 2.91 s. Direct deterministic reset-lifecycle test passed in approximately 0.22 s.
- ASan+UBSan focused Audio1: `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/audio-sanitize --output-on-failure -L audio -j1` — 7/7 passed in 3.96 s; isolated runtime 2.25 s and reset lifecycle 0.29 s. This includes the existing 250-immediate-start/stop production-backend FD/callback barrier.
- Release production build and broad registry: `cmake --build build/audio-release --parallel 2 && ctest --test-dir build/audio-release --output-on-failure -j2` — 90/90 passed in 11.92 s; Audio1 7/7.
- Debug production build and broad registry: `cmake --build build/audio-dev --parallel 2 && ctest --test-dir build/audio-dev --output-on-failure -j2` — 90/90 passed in 11.36 s; Audio1 7/7.
- After strengthening exact epoch assertions, the reset-lifecycle executable was rebuilt and passed in Debug, Release, and ASan+UBSan: 3/3 QtTest cases in each configuration, approximately 0.22–0.26 s.
- Exact committed lifecycle repetition: `ctest --test-dir build/audio-dev -R '^qindaqt\.audio-(activation|wireplumber-runtime|wireplumber-reset-lifecycle)$' --repeat until-fail:10 --output-on-failure` — activation 10/10, full isolated runtime 10/10, reset lifecycle 10/10; 30 test executions passed in 21.49 s.
- `cmake --install build/audio-release` — staged install passed and updated the public Audio1 libraries/headers, executable, D-Bus activation descriptor, and hardened user unit. Manual activation used only a private bus and private XDG/PipeWire roots: exact staged executable PID `3892853` acquired the name; private bus death removed that exact process within the bounded poll.
- `./tools/validate-docs` — 43 documents/navigation passed; `uvx --from mkdocs mkdocs build --strict` passed.
- `./tools/check-source-shape` — 748 files checked, zero allowlist skips; changed `wireplumber_worker.cpp` is 484 nonblank lines. `git diff --check` and the cached whitespace check passed. No QML registry changed, so `all_qmllint` is not applicable.
- Final exact cleanup audit after the committed repetitions found zero Audio1 candidate executables in `/proc` and zero matching private fixture roots in `/tmp`; `git status --short` is empty.

All D-Bus, PipeWire, and WirePlumber evidence used private daemons, temporary XDG runtime/config/state roots, a private PipeWire socket, disposable null devices, and synthetic sample generation. No host session bus, host PipeWire/WirePlumber graph, physical microphone/speaker, desktop, input, or user/system configuration was touched.

## Bounded caveats and next ownership

The deterministic lifecycle test is a scheduler/lifetime regression, not a long soak. The prior 250-cycle unavailable-runtime test remains a bounded callback/FD barrier (allowed delta at most five), not latency, CPU, PSS/RSS, wakeup, realtime, or sustained-churn qualification. The service still uses one Qt process plus one confined GLib worker thread, and upstream PipeWire/WirePlumber remains runtime/policy authority.

USB, HDMI, Bluetooth, jack, multichannel, microphone, hardware hotplug, suspend/resume, hardware gain mapping, real-device behavior, and physical audio quality remain unqualified. The ratified Settings route `audio`, narrow default-output shell facade plus `openAudioSettings()`, shared availability extraction after two accepted producers, and all accessibility/visual/integrated UI proof remain with future owners; this follow-up adds no page/applet/UI.

Requested next action: the same independent reviewer should review exact commit `bd3a94e32aff5a5bd8bde737aae62e8330241734` and tree `f7d01c8b54aba090be7a21ebaf98f782d3348bea`, specifically reproducing the original loss-idle/stop/restart/second-loss order and checking the full lifecycle/sanitizer gates, then post exact accept/reject evidence. Review the hash, not this prose, and do not integrate before that verdict.

— Noor Hale, Audio1 implementer
