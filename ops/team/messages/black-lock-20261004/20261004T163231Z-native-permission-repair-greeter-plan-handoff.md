# Production native greeter contained plan handoff

- Timestamp: 2026-10-04T16:32:31+00:00
- Worker: `/root/native_permission_repair`
- Status: available; no owned runtime launched
- Candidate source untouched: `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae`
- Temporary helper SHA256: `b9a1eb0aede08b595202e678cf0ca92932db30d5001d4928927eac66dfe3efb1`

Files are only in ignored worker cache and qinda private cache:

- Laptop: `/home/cabewse/work_space/qindaqt-kwin-lock-permissions/.cache/native-production-greeter/qualification.py`, `PLAN.md`, `plan.json`
- qinda: `/home/cabewse/.cache/qindaqt-native-greeter-qualification/qualification.py`, `PLAN.md`, `plan.json`, `verified-plan.json`

Actual prerequisite evidence: retained Portage r6 image executable and core library root:root0755, no RPATH/RUNPATH, pinned SHA256; CMakeCache `KWIN_BUILD_SCREENLOCKER=ON`, `QINDAQT_SESSION_LOCK_TEST_AUTHORIZATION=OFF`, `BUILD_TESTING=OFF`; fixed production paths in compile definitions. qinda has non-setuid bubblewrap0.11.2-r1, Python3, sudo-n, private dbus-run-session, busctl JSON/address support, timeout and Yama1. Correct build executable path is `work/qindaqt-kwin-6.6.6_p1-r6_build/program/qindaqt-kwin`. Exact hashes and full mount plan are in the helper/PLAN.md.

Execution proposal, **only after manager authorizes**:

```sh
ssh qinda 'python3 /home/cabewse/.cache/qindaqt-native-greeter-qualification/qualification.py --execute'
```

Default invocation is plan-only. It and AST parsing passed exit0 locally; nine synthetic strict trace cases passed; mount/private-bus/PAM/software-plan invariants passed. Copy and remote default plan/hash verification passed exit0. No namespace, bus, compositor, GPU, PAM or live endpoint has run for this preparation. These are static preparation checks, not runtime acceptance.

Plan: root-created mount/PID/IPC/network/UTS namespace retaining initial UID map, then groups/GID/UID1000 drop/no-new-privileges/zero capability assertion before private bus/compositor. Installed `/usr` and r6 image prefix read-only, host `/run`/sysfs/physical devices hidden, only five safe basic device nodes present. Private public NSS identity entries have placeholders; no credential/shadow/PAM configuration mounted. Zero service-activation bus directories; explicit private address, no auto-start/interactive authorization, native owner PID pin. Fixed PAM worker alone masked with nonregular node as manager authorized, so real greeter automatic authentication cannot spawn PAM. Supported QPainter virtual1280x720 compositor; no fixture or no-lockscreen flags.

Pass requires RequestLock=true, actual fixed-image direct native greeter child, configured/acknowledged/non-null-buffer committed standard lock surface plus protocol locked, Locked/Protected=true, r6 loader-init prefix/hash, all stable at least0.5s. Protected alone cannot pass. Optional frame callback recorded. Exact private connection follows production's unchanged filtered-global/LockerLauncher identity contract; SO_PEERCRED creator PID is not misread as child identity.

Caveats: no PAM acceptance, credentials, unlock, physical scanout/pixel appearance/live session restart. Graphics overrides do not penetrate the real launcher's sanitized environment; lack of device-free QtQuick rendering is a failed/limited gate. Privileged bwrap initial-user-namespace/capability behavior remains unexecuted and must pass the bootstrap assertions. Parent should verify no owned namespace/helper survives after the bounded45s+5s timeout and process-group/reaper cleanup.

Requested next action: manager reviews this concrete helper/privilege plan and authorizes its bounded contained execution. Compatible help offer: I can execute this exact helper after authorization and report its exact evidence, or repair its observed failure within the same ignored helper only.
