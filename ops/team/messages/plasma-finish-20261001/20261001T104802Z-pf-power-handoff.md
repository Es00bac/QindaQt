# Native protected sleep candidate handoff

- Worker: pf-power-sol-20261001
- Time: 2026-10-01T10:48:02Z
- Immutable verified implementation: `3e9fcaccad6b65fa5d872b36d8e6e0012284d4a6`
- Exact base: `53bf486bac9c067e5fff8143e2457e69e4a7b01d`
- Branch: `worker/pf-power-sleep-20261001`
- Laptop worktree: `.cache/pf-power-sleep-20261001`
- qinda worktree: `~/work_SPaC3/container-wm.worktrees/pf-power-sleep-20261001`
- Candidate status: complete bounded source/wire outcome; waiting for independent review, not integrated
- Bookkeeping: this reply/employee waiting state are source-identical metadata-only descendants of the immutable verified implementation; manager receives exact final tip separately
- Next action: different worker reviews exact supplied candidate, then Program Manager integrates accepted commit and reruns affected gates
- Resources: qinda compiler and private-bus runtime released

## User-visible outcome

Manual SessionActions suspend routes solely through supervisor-owned Sleep1, whose current unique owner must equal Session1. The coordinator waits for authenticated targeted native Locked/Protected state before logind dispatch and rechecks protection after CanSuspend. No direct SessionActions login1.Suspend mapping or stock locker fallback remains.

The logind adapter validates root unique owner, explicitly selected session ID, real supervisor PID/GetSessionByPID, exact Id/User `(uo)` UID/canonical object path, and live supervisor/ordinary attachment admission. It owns a CLOEXEC sleep delay FD. Selected Lock requests native admission; Unlock never authenticates; LockedHint follows authenticated Unlocked or protected Locked only. PrepareForSleep retains the FD until current native protection and rearms on resume. Selected session removal, owner/peer/supervisor/bus loss, stop/restart and late replies revoke safely. Unknown/incomplete/failed protection never supplies success. Dispatched sleep without confirmation returns explicit Uncertain, never replay. Public facade stop cancels pending work; stopped runtime preferences cannot rearm idle handles. Power1 supported inhibitor scopes remain zero.

Logind's delay has a finite system deadline: this gate does not veto privileged external sleep after that deadline. Idle-suspend/lid/button source policy is not implemented by this outcome.

## Executable acceptance

The approved qinda configure is Debug/shared ON/strict warnings ON/BUILD_TESTING ON/plugin OFF, ccache, host-uinput OFF; no ABI qualification is claimed. Actual `portageq envvar MAKEOPTS` returned unchanged `-j24 -l24`.

```sh
ninja -C build/focused -j24 -l24 qindaqt_logind_sleep_transport_tests qindaqt_sleep_coordinator_tests qindaqt_native_lock_runtime_tests qindaqt_session_actions_tests qindaqt_qt_native_lock_request_tests qindaqt_session_supervisor_support
ctest --test-dir build/focused --output-on-failure -V -R 'qindaqt\.(logind_sleep_transport|sleep_coordinator|session-native-lock-runtime|session-actions-client|session-actions-boundary|session-actions-boundary-poison|qt_native_lock_request)$'
mkdocs build --strict
tools/validate-docs
git diff --check
```

- Repaired qinda build: exit 0; 29 actual actions, strict warnings; source exact 3e9fcaccad6b65fa5d872b36d8e6e0012284d4a6.
- Repaired CTest: exit 0, **7/7 rows**, 34.68s, no skips.
- Qt raw totals (including init/cleanup): SessionActions **10/10**, Runtime **12/12**, Logind **17/17**, SleepCoordinator **16/16**, NativeRequest **16/16**; **71 passed, 0 failed, 0 skipped**. Boundary and poison are two additional CMake rows.
- Tests use disposable private brokers, real ordinary socket/PIDFD attachment and production targeted nonce native transport/request ports, plus pipe-backed Unix FDs. No real host system bus, lock, sleep, passwords or session restart.
- Strict MkDocs exit 0; `tools/validate-docs` exit 0, **480** documents/navigation; diff-check exit 0.
- Focused standard source-shape CLI: exit 0, **29** changed source files, 0 exceptions/issues, `--warnings-as-errors`. Stage only changed source paths preserving relative paths under ignored `build/evidence/changed-source`, then execute `tools/check-source-shape --root build/evidence/changed-source --config tools/source-shape.json --warnings-as-errors --largest 3`. Largest production source is SessionActions client at 400 nonblank lines; its availability query is split into a separate 142-line cohesive source.
- Full repository shape was run and retains unrelated existing failures; no cleanup/reformat of unowned paths was made.

Preserved qinda evidence: `build-configure.log`, `build-repaired.log`, `test-repaired.log`, and `build/focused/Testing/Temporary/LastTest.log` in the assigned worktree. Local ignored evidence: `build/evidence/mkdocs-final.log`, `validate-docs-final.log`, `source-shape-cli.log`. Earlier initial 9add7dab 7/7 pass and expanded c7427d07 two failures (watcher self-delete crash and client fixture/deadline) remain in `test-focused.log` and `test-acceptance.log`; they are not substituted for repaired final evidence.

## Qualification limits

Actual installed logind timing, physical DRM presentation, hardware sleep/resume, PAM and native scene mapping are not tested here. Parent manages the accepted compositor stage and combined ABI/nested gate independently. Plugin-OFF ordinary wire tests cannot establish compositor ABI or physical protection. No package, overlay, live bus, host action, manager Task/Handoff/features/queues or idle source policy was changed.

## Changed paths

- `docs/wiki/adr/0321-supervisor-owned-native-sleep-admission.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/native-session-lock.md`
- `docs/wiki/architecture/power-service.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/session1-v1.md`
- `docs/wiki/shell/power-applet.md`
- `mkdocs.yml`
- `ops/team/messages/plasma-finish-20261001/20261001T095526Z-pf-power-claim.md`
- `ops/team/messages/plasma-finish-20261001/20261001T101200Z-pf-power-implementation.md`
- `ops/team/messages/plasma-finish-20261001/20261001T102118Z-pf-power-verification.md`
- `ops/team/messages/plasma-finish-20261001/20261001T102741Z-pf-power-repair.md`
- `ops/team/workers/pf-power-sol-20261001.md`
- `src/CMakeLists.txt`
- `src/services/session_actions/CMakeLists.txt`
- `src/services/session_actions/include/qindaqt/services/session_actions/session_actions_client.h`
- `src/services/session_actions/src/session_actions_availability.cpp`
- `src/services/session_actions/src/session_actions_client.cpp`
- `src/services/session_actions/src/session_actions_internal.h`
- `src/services/session_actions/src/session_actions_suspend.cpp`
- `src/session/native_lock_runtime/include/qindaqt/session/native_lock_runtime/native_lock_runtime.h`
- `src/session/native_lock_runtime/src/native_lock_runtime.cpp`
- `src/session/native_sleep/CMakeLists.txt`
- `src/session/native_sleep/include/qindaqt/session/native_sleep/logind_sleep_transport.h`
- `src/session/native_sleep/include/qindaqt/session/native_sleep/sleep_coordinator.h`
- `src/session/native_sleep/include/qindaqt/session/native_sleep/sleep_service.h`
- `src/session/native_sleep/src/logind_sleep_transport.cpp`
- `src/session/native_sleep/src/sleep_coordinator.cpp`
- `src/session/native_sleep/src/sleep_service.cpp`
- `src/session_supervisor/CMakeLists.txt`
- `src/session_supervisor/src/native_lock_composition.cpp`
- `src/session_supervisor/src/native_sleep_composition.cpp`
- `src/session_supervisor/src/native_sleep_composition.h`
- `tests/CMakeLists.txt`
- `tests/services/session_actions/check_boundary.cmake`
- `tests/services/session_actions/tst_session_actions_client.cpp`
- `tests/session/native_lock_runtime/tst_native_lock_runtime.cpp`
- `tests/session/native_sleep/CMakeLists.txt`
- `tests/session/native_sleep/fake_logind_wire.h`
- `tests/session/native_sleep/sleep_test_support.h`
- `tests/session/native_sleep/tst_logind_sleep_transport.cpp`
- `tests/session/native_sleep/tst_sleep_coordinator.cpp`
