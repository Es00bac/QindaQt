# Installed adoption verified — 2026-10-17T18:11:42Z

- Reviewer: post-reboot-radio-review-codex-20261017, independent of implementer.
- Accepted source pins: Desktop `37988eb8fc6a990f475b815e10c6b0c596437d84`; Gabbee `be99a1b5f84fc60a6469c8e76cd5f6240250800b`; overlay `8dfc954ccbf29514b2433c3bb54fddfb35e31263`.
- Audit inspected: manager worktree `docs/wiki/development/reboot-service-audit-20261017.md`, SHA256 `f854659aadeff5cf64f9d70be223193a6aace392454699afbefed3700f852672`.
- Blocking discrepancy in the reviewed adoption claims: none.

Direct read-only observations:

1. `/var/db/pkg/app-accessibility/gabbee-0.1.0_p20261002-r8/CONTENTS` verifies 262/262 regular objects against recorded MD5, 28 directories and one symlink: 291 records total, zero mismatches. Installed `voice_watchdog.py` is byte-identical to exact accepted Gabbee source; SHA256 `ea2e2a96e408fc3fa3aa995fa4c23c0157f6c6154d65092f1e61009afd81585b`.
2. The installed watcher's read-only `shortcut_session_present` probe returns true for the current provider and native backend. The provider is idle, enabled, reason `ok`. The probe was constructed without the report-writer initializer; no watchdog recovery action was run.
3. Public `Shortcuts1.ListBindings` contains two active Gabbee bindings: dictation `Media Next` and command `Media Stop`. Only kind, active flag and key text were retained; private native IDs and complete replies were not printed.
4. `systemctl --user show qindaqt-bluetooth-radio-helper.service` reports active/running, Result=success, ExecMainStatus=0, MainPID=318823. `GetNameOwner(BluetoothRadio1)` returns `:1.32030`, and its broker-reported PID matches that exact unit MainPID.
5. The helper's only reported drop-in is the scoped per-user `90-reboot-bus-visibility.conf`. Its executable settings are exactly `ProtectHome=tmpfs` and `BindReadOnlyPaths=%t/bus`. Effective properties show only the runtime bus socket read-only bind plus the existing `/dev/rfkill` bind, PrivateUsers/PrivateDevices, NoNewPrivileges, closed device policy for `/dev/rfkill`, and AF_UNIX restriction.
6. Public `Bluetooth1.GetSnapshot` reports one adapter, powered=true, and one connected device. Device names and identities were discarded. No radio method was called by this reviewer.
7. System and user `systemctl --failed --no-legend --no-pager` both exit 0 with empty output. Latest watchdog oneshot reports inactive/dead between runs, Result=success and ExecMainStatus=0.
8. Laptop package metadata still lists Desktop r30, with no r31. On qinda, **system-scope** `qindaqt-reboot-repair-build-20261017.service` reports activating/start, MainPID=2070427, CPUQuotaPerSecUSec=8s, MemoryMax=12884901888 and MemorySwapMax=0. This is pending build evidence, not a successful build or installed r31 claim.

Every direct read-only check above exited 0. The audit accurately distinguishes
installed Gabbee r8, effective user-level Bluetooth configuration and pending
Desktop r31 delivery. The manager's earlier successful already-powered
SetPowered(true) call is manager-supplied evidence; it was deliberately not
repeated by this reviewer. Current readback independently confirms the powered
adapter and connected-device count only.

Physical Next Track activation/text delivery and a newly exercised selected-index
helper write remain unconfirmed. Sleep early-wake behavior and OOM workload
attribution remain unresolved. The audit preserves those limits and does not
claim suspend caused the shortcut authority loss. No package install, service
restart, radio toggle, microphone activation or suspend was performed.

Requested next action: finish and verify the single capped Portage build,
adopt r31 through Portage, remove only the matching incident override when the
packaged default replaces it, then verify the effective default and owner again.
The reviewer is available for bounded artifact/default-unit adoption review;
no background test or service mutation is left running.
