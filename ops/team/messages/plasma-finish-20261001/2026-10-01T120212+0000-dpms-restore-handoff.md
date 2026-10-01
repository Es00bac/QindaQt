# DPMS restore repaired candidate handoff

- Candidate: `d08e49d13747cb0b71c3f13abfc8ecf058c46a4c`
- Base: `52c08cdb9f6fa0181beebdd02b42a54af78c2ba2`
- Preserved hub branch: `qinda:~/git/container-wm.git` / `worker/pf-dpms-restore-20261001`
- Worker: pf-dpms-restore-sol-20261001
- Status: waiting for exact independent review
- Timestamp: 2026-10-01T12:02:12+00:00
- Requested action: review exact candidate, then manager integrates and reruns affected combined/native gates.

## Changed paths

- `src/session/idle_policy/production/kwayland_dpms_controller.cpp`
- `tests/session/idle_policy/dpms_wayland_fixture.cpp`
- `tests/session/idle_policy/dpms_wayland_fixture.h`
- `tests/session/idle_policy/tst_kwayland_dpms_controller.cpp`
- `docs/wiki/architecture/idle-policy.md`
- `docs/wiki/development/testing-harness.md`
- `ops/team/workers/pf-dpms-restore-sol-20261001.md`
- New timestamped claim and causality messages in this thread.

## Causality and repair

KWayland requestMode always marshals the request; cache suppression ruled out. Wayland 1.24 source from installed Portage distfile proves server HANGUP destroys a peer before READABLE handles buffered requests. Original timing-altered WAYLAND_DEBUG trace passed, which did not clear the blocking combined failure. An actual private socket-read pause for 80 ms then deterministically failed both live/revoked pre-repair rows: client marshalled On, server saw only the earlier Off. Pre-repair expected count2 versus actual1, 2 failed rows, exit2.

Repair retains the same descriptor until ordered sync acknowledgement or one 250 ms deadline. A dedicated worker-only queue prevents GUI-owned KWayland callbacks from dispatching on the blocked worker; callback and queue are destroyed before existing proxy/display teardown. Ordinary requests after revocation remain forbidden; final restore opens no replacement connection. No new dependency, public/process boundary or ADR.

## Exact commands and evidence

All product compile/runtime commands ran in the worker's own qinda worktree `~/work_SPaC3/container-wm.worktrees/pf-dpms-restore-20261001`.

- `cmake -S . -B build/dpms -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQINDAQT_ENABLE_STRICT_WARNINGS=ON -DQINDAQT_BUILD_KWIN_PLUGIN=OFF`: exit0.
- `cmake --build build/dpms --target qindaqt_kwayland_dpms_controller_tests qindaqt_idle_display_stage_tests -- -j24 -l24`: exit0; original 66 actions and final affected rebuild pass. MAKEOPTS unchanged.
- `QT_FATAL_WARNINGS=1 WAYLAND_DEBUG=1 build/dpms/tests/session/idle_policy/qindaqt_kwayland_dpms_controller_tests admittedConnectionTracksCapabilityRemovalAndRestore`: original 3 Qt cases passed, exit0; timing perturbation only (`original-trace.log`).
- `QT_FATAL_WARNINGS=1 WAYLAND_DEBUG=1 build/dpms/tests/session/idle_policy/qindaqt_kwayland_dpms_controller_tests finalRestoreWaitsForDelayedPeer`: pre-repair live/revoked both fail; 2 passed lifecycle cases, 2 failed behavior rows, exit2 (`held-peer-before-repair.log`).
- `ctest --test-dir build/dpms --output-on-failure -R '^qindaqt.(kwayland-dpms-controller|session-idle-display-stage)$'`: repaired 2/2 CTests pass, exit0; idle stage10 Qt cases, DPMS8 Qt cases. DPMS original request-count5/last-On assertions unchanged, new delayed live/revoked rows pass, stalled peer exercises expiry (elapsed150–750ms bound around250ms), absent/disconnected peer teardown passes.
- Laptop isolated worktree `mkdocs build --strict --site-dir build/docs/site`: exit0.
- `tools/validate-docs`: exit0, 481 Markdown documents/navigation validated.
- `git diff --check`: exit0.

Evidence logs: `build/dpms/{configure.log,initial-build.log,repro-build.log,held-peer-before-repair.log,original-trace.log,repair-build.log,repaired-trace.log,final-build.log,final-ctest.log,Testing/Temporary/LastTest.log}`. Source research retained under ignored `build/research`.

## Bounded caveats and release

Best effort remains best effort if the peer never dispatches or dies; timeout does not manufacture acknowledgement. Scheduling and missing acknowledgement are exercised; artificial socket-buffer saturation is not separately induced. No host display, bus, input, packages, native session restart or physical-output qualification performed. Root owns remaining combined/plugin/native gates. Compiler/private-runtime resources released to root before this handoff. Own qinda worktree fast-forwarded to candidate; its prior compiled source edits are also preserved in a named Git stash (same source).

Read the Platform queue after handoff; concrete help offer: independently reproduce any exact DPMS reviewer finding in this same worktree, without claiming new paths/resources. Waiting until routed; no new product outcome claimed.
