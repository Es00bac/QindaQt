# Native sleep modes immutable candidate handoff

- Time: 2026-10-01T14:30:34+00:00
- Worker: pf-native-sleep-modes-sol-20261001
- Exact candidate: **50fb9c11620df99122665b3d6bdbe39684d003c0**
- Exact base: e21c1757d45ee4814734ad646a8c95b2c5b14b00
- Strictly compiled/runtime-tested source: 052697d3decadc6fef1139282f6a46804989d5ab
- Source identity: git diff --quiet 052697d3 HEAD -- src tests exit0; candidate differs only docs/own operational records
- Branch: worker/pf-native-sleep-modes-20261001
- Laptop worktree: .cache/pf-native-sleep-modes-20261001
- qinda worktree: /home/cabewse/work_SPaC3/container-wm.worktrees/pf-native-sleep-modes-20261001
- State: executable bounded candidate, awaiting different-worker review; not integrated, not PF2 complete
- Resources: compiler/private runtime released; actual known run PIDs/own markers absent
- Next manager action: independent exact-candidate review, then integrate accepted source and rerun affected gate; adopt ADR0325 after acceptance

## Outcome and public contract

Typed SleepMode selects exactly Suspend, Hibernate, HybridSleep and SuspendThenHibernate. Existing requestSuspend APIs and Sleep1 Suspend/CanSuspend zero-input boolean-output ABI remain compatible. Add Hibernate/CanHibernate, HybridSleep/CanHybridSleep and SuspendThenHibernate/CanSuspendThenHibernate on the same versionless interface; no invented version property or arbitrary method string dispatch. All actions share one pending mutation, retained CLOEXEC delay FD, selected root-logind identity/session join, ordinary attachment and current targeted Locked/Protected receipt admission. Repeat exact mode Can only after protection, recheck authority/protection, then send only that action with interactive=false. Challenge/no/na/missing/malformed or contradictory replies refuse, with no fallback mode, unprotected call, alternate locker or shell command.

Can methods authenticate actual session-bus caller UID, require own live Session1 owner and query the current selected transport mode capability. They are current hints, never protection/reservations. Logind/polkit sees the real supervisor connection credentials; Sleep1 does not impersonate client PID or delegate another process's PID-specific grants. Same-UID only, bounded16 readonly queries,250ms credential/capability bus calls plus existing bounded owner checks. Unknown/Locking, pending mutation and missing delay descriptor refuse. Explicit cancellation retires exact request serial before another mode can start; old Can/action replies cannot dispatch/complete new work in the same logind epoch. Loss/error/cancellation after dispatch is Uncertain/no replay. All previous native unlock/resume/FD/epoch rules remain. Source-specific idle/lid/critical-battery Power policy and milestone completion remain separate.

## Build and exact execution

Actual installed qinda Portage MAKEOPTS was -j24 -l24, unchanged. Own configure:

```sh
cmake -S . -B build/native-sleep-modes -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON -DQINDAQT_ENABLE_STRICT_WARNINGS=ON -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_PREFIX_PATH=/home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/pf-knighttime-gate/stage/usr
cmake --build build/native-sleep-modes --target qindaqt_logind_sleep_transport_tests qindaqt_sleep_coordinator_tests qindaqt_sleep_modes_tests qindaqt_native_lock_runtime_tests qindaqt_session_actions_tests qindaqt_qt_native_lock_request_tests qindaqt_session_supervisor_support -- -j24 -l24
ctest --test-dir build/native-sleep-modes --output-on-failure -V -R '^qindaqt\.(logind_sleep_transport|sleep_coordinator|sleep_modes|session-native-lock-runtime|session-actions-client|session-actions-boundary|session-actions-boundary-poison|qt_native_lock_request)$' -j1
```

The CTest command ran inside the explicitly approved readonly private bwrap namespace, not with installed activation directories. Actual argv is in build/native-sleep-modes/private-qualification/retry-command.json, with check_identity-retry.py preserving the preflight/execution recipe. Mounts: --ro-bind / /, writable own ignored build only, private --tmpfs /tmp, readonly task empty-session.conf over /usr/share/dbus-1/session.conf, and ONLY --dev-bind /dev/null /dev/null to restore ordinary null access. No PID namespace remap. HOME, XDG_CONFIG_HOME/DATA_HOME/DATA_DIRS/CACHE_HOME/RUNTIME_DIR point to private task roots; DISPLAY/WAYLAND_DISPLAY/WAYLAND_SOCKET/session bus/ambient Qt themes cleared, DBUS_SYSTEM_BUS_ADDRESS points to unavailable private path (per-test absent overrides also retained), QT_FATAL_WARNINGS=1 and offscreen. RLIMIT_CORE=(0,0) set before subprocess. Actual guard records UID/eUID/GID1000, uid/gid maps1000→1000, private tmpfs0755 owned1000 and successful private write/null O_RDWR, readonly session.conf mount and exact hashes. Installed session.conf SHA256206f009ddcf909422f3651c687b4623a5780fb7486c6d650e5332f57c00d6be1 unchanged before/after.

- Strict configure/build exit0; **330 actual Ninja action lines**,0 FAILED markers; seven exact focused targets.
- Corrected CTest exit0, **8/8**,60.10s,0 failed/skipped.
- Actual Qt totals including lifecycle: SessionActions10, Runtime12, Logind17, Coordinator16, Modes72, NativeRequest16: **143 passed,0 failed,0 skipped**. Two boundary/poison CTest rows are additional non-Qt rows.
- All70 mode behavior/data cases plus2 lifecycle checks cover every mode's pairing/protection, unsupported/challenge/no/type failure, actual caller denial, old serial/new mode, owner loss before/after dispatch, timeout/malformed action uncertainty, changed capability, delayed protection loss, forged current nonce, unknown/Locking, duplicate/pending, descriptor closure and stop. Original suspend assertions retained.
- tools/validate-docs exit0,484; strict MkDocs exit0; focused shape src8/tests7 exit0, largest production272/test228; diff-check exit0. Final docs record candidate evidence, not integration.

## Preserved failure and host-artifact history

First namespace run exit8:3/8 CTests passed in14.00s; five bus-based rows failed during setup. Actual dbus-daemon stderr was Failed to open /dev/null: Permission denied because readonly root is nodev. Initial tmpfs-permission guess was unproven; actual corrected tmpfs0755 UID1000 is writable. No production mode defect was established and no admission/assertion was weakened. The null-only device bind corrected this isolation failure; no fallback to installed activation directories. The first run's downstream fixture asserts aborted two processes and generated host systemd-coredump files. Blanket no-host-artifact claims do not apply to this history.

Direct raw failure backtraces identify PIDs2711280/2711314. Selected nonsecret file xattrs independently match own executable/PID/UID1000/signal6/timestamp2026-10-01T14:08:21Z. Manager explicitly authorized preserving compressed bytes and removing ONLY these task-generated originals. Ordinary UID copied opaque compressed bytes without decoding/inspecting core contents/private env; copies mode0600 in own ignored failure-artifacts. The sudo-n helper reverified inode/size/mtime/PID and both original/copy SHA before unlink, exit0, neither original remains. Inodes22560848/22560849; compressed SHA256 e79e0fa2ae257668559ad042b781ecd04c0038ac5d071734387d5446b84631ca and15ca5f190ab39825fe3a47564c362023cee681f61f90be95e30413afc72a87ed. Unrelated older core2553023 was untouched; no unrelated service stopped or system setting modified.

Corrected run started before the manager's later core-limit precondition message reached the turn boundary and had already completed successfully when the exact descendant tree was inspected (no descendants/signals). Subsequent direct installed systemd261.2 primary source verified coredump-context.c parses kernel %c limit and coredump-submit.c:265 returns before storage/processing for rlimit<page_size. RLIMIT0 therefore suppresses saved cores; it does not prohibit piped handler invocation/journal metadata on a crash. Corrected run had no crashes or additional matching core artifacts. This ordering and first-run host artifacts remain disclosed; no source qualification is inferred from an interrupted or skipped gate.

## Evidence and limits

Own qinda build/native-sleep-modes retains configure-first.log/build-first.log/first-build-result.json, tests-first.log (failure), tests-null-restored.log (passing), private-qualification/command.json/namespace-identity.json/retry-command.json/retry-namespace-identity.json/check_identity-retry.py/final-lifetime-audit.json and failure-artifacts/core-preservation.json/core-cleanup.json/primary-core-limit.json plus two opaque .zst copies. Exact file hashes/stat/precondition and cleanup results are durable ignored evidence. Installed systemd261.2 and dbus1.16.2 primary distfile sources were inspected directly; no inferred provider/model or fresh ABI claim.

These are actual private process/wire gates with daemon credentials, ordinary Unix socket/kernel PIDFD and production native Qt nonce ports backed by fixture receipts/logind pipe FDs (fixture daemon expected UID explicitly injected, production still root UID0). They do not qualify actual host logind/polkit timing, physical output protection, hardware support/hibernate images/resume, real PAM/unlock or installed deployment. No host sleep/lock/authentication, Settings/Power policy, production shell/compositor, portal/capture, DPMS, overlay, manager queues/features or install change was made. No program/milestone advance is claimed.

## Changed paths and bounded help

Product changes are confined to src/session/native_sleep (typed mode header/transport/coordinator/facade and local CMake), tests/session/native_sleep (mode test/empty private broker/shared fixture and local registration), native lock architecture/testing/reference docs, ADR0325 plus approved exact ADR index/mkdocs addition. Own worker/messages are operational only. No supervisor composition/consumer change was needed.

Read the Platform queue and relevant prior sleep/privacy peer replies. After handoff, offer one bounded exact reviewer-finding reproduction/repair within these owned paths with fresh resource grant. Do not silently claim later lid/critical/idle policy. Worker is waiting; manager should route exact candidate review and integrated gate.
