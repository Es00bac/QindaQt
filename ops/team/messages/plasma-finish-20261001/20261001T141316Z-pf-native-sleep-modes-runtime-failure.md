# Preserve private namespace bus setup failure

- Time: 2026-10-01T14:13:16+00:00
- Exact source: 052697d3decadc6fef1139282f6a46804989d5ab
- CTest: exit8,3/8 pass,5 failures,14.00s
- Compiler/private runtime: released; capture compiler next

First namespace runner passed UID/eUID/GID1000 identity guard (uid_map1000→1000, no PID namespace change), readonly private session.conf SHA256/mount guard. Installed input SHA256206f009ddcf909422f3651c687b4623a5780fb7486c6d650e5332f57c00d6be1 unchanged before/after. However actual dbus-run-session stderr is `dbus-daemon: fatal error setting up standard fds: Failed to open /dev/null: Permission denied`. Readonly root bind is nodev; proposed null-only dev-bind restores ordinary null access without host input/uinput or installed service dirs. Earlier tmpfs permissions hypothesis was unproven and is corrected.

SessionActions failed before Qt initialization; native sleep broker registration failed, with two downstream fixture QList index asserts aborting (Logind1pass/8fail, Coordinator1pass/8fail); mode fixture2pass/70fail and NativeRequest2pass/14fail before ready. Runtime12Qt passes and two boundary CTest rows pass. Those are failed verification, no source-mode regression is yet established. All original outputs remain in tests-first.log and private-qualification/command.json/namespace-identity.json; no failed results substituted or skipped.

Prepare exact runner correction with only /dev/null dev-bind and preflight O_RDWR null/private temporary creation, actual tmpfs mode/UID evidence, currentUID/GID and mount guards, core disabled, same strict immutable binaries, installed conf input hashes. Once capture releases compiler, request fresh bounded8-row private retry. No fallback to installed activation dirs, no production admission weakening and no host sleep/lock/PAM/services/file changes.
