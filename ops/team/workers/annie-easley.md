---
name: Annie Easley
role: Bluetooth applet B1 implementer
provider: OpenAI Codex
model: inherited exact serving model; unexposed
reasoning: inherited exact reasoning level; unexposed
status: working
feature: QQ-004.14 production Bluetooth applet B1
worktree: /home/cabewse/work_SPaC3/container-wm-workers/bluetooth-applet-b1
started_at: 2026-08-31T06:00:28-06:00
updated_at: 2026-08-31T16:16:57-06:00
---

# Annie Easley

Implements the production Bluetooth applet over the public Bluetooth1/BluetoothClient B0 boundary.

- Status: working — both manager-reported operation-lineage defects are repaired with mutation coverage; rerunning static gates before fresh Debug/Release qualification.

## Updates

- 2026-08-31T16:16:57-06:00 — Manager audit found a second stale-truth mutation window: after valid success at observed revision 6, the controller cleared its request while the public client still exposed initiating snapshot revision 5, re-enabling the same disconnect/power/connect action before the async refetch. Added an exact-owner/epoch/minimum-revision success-convergence fence to pending-equivalent control admission. A controller mutation test proves the old revision cannot dispatch a second disconnect, an equal initiating snapshot cannot clear the fence, and only revision 6 truth with the disconnected device re-enables the now-valid Connect action. Owner/state loss ends the fence uncertain and fail-closed; there is no replay. Static gates are being rerun before compiler use.

- 2026-08-31T16:09:48-06:00 — Ordinary merge `f23b61d91fdf76f6e4cecaa87808a16dd48f116b` now preserves B1 and exact manager main `74da46345c7a5094d45c756ad8b23ca87591fcd3`; the single roadmap conflict retained S3/Network/Portal truth plus B1. Manager static audit then identified that `handleOperationCompleted` trusted raw `no-lease` text after `applyBluetoothResult` could classify the completion uncertain. Removed that raw-string lease retirement, added a controller mutation regression for a wire-invalid matching request with `no-lease`, and clarified that only validated success or authoritative snapshot truth retires the lease. Verification is pending; no private runtime or bus was used.

- 2026-08-31T16:04:03-06:00 — Program Manager explicitly released the compiler/CTest lane after direct process inspection and assigned ordinary merge of exact integrated main `74da46345c7a5094d45c756ad8b23ca87591fcd3` into preserved B1 milestone `c2cf9a0066e0175a99b1dfaea0735ae2569794b8`. Resumed in the existing isolated worktree; scope remains public Bluetooth1/client-only shell composition, with private D-Bus, nested compositor, BlueZ, host Bluetooth, hardware, network, and input lanes prohibited. The next gates are additive seven-file conflict resolution, mutation-sensitive static checks, then fresh serialized strict Debug/Release focused and adjacent rows.

- 2026-08-31T07:33:52-06:00 — Program Manager confirmed Network Settings integrated on main at `01145dcd5886861657a348b3e5b18a75fa7c6307` and assigned S3 the next serialized repair/review/runtime sequence. B1 stays source-static and must not configure/build/CTest. After S3 integration, B1 will ordinary-merge latest main before executable gates and candidate freeze; no pre-integration merge or lane use was attempted.

- 2026-08-31T07:27:29-06:00 — Static milestone remains clean at `c2cf9a0066e0175a99b1dfaea0735ae2569794b8`. Dorothy has terminally handed off S3 and reported its runtime lane released, but the Program Manager's earlier instruction requires an explicit B1 release after that transition. Requested that authorization and remain idle at the boundary; no configure/build/CTest/private runtime was started.

- 2026-08-31T06:55:23-06:00 — Completed the permitted source/docs/static audit. The direct pure and runtime boundary gates pass over explicit five-file/seven-file inventories and reject copied-tree client/service/pairing poison; discovery release admission now also fails closed if its current adapter vanished. Documentation validates across 112 Markdown files, pinned MkDocs strict succeeds, `git diff --check` passes, and source shape checks 1,715 files with only the three pre-existing unrelated threshold warnings. No configure/build/CTest/private bus was used. Per Program Manager coordination, the only source-test import additions outside the new focused directory are dispatcher-only `QindaQt/Shell/BluetoothApplet` QML doubles: they satisfy the new production import when the existing `qindaqt.notification-center-applet-offscreen` row source-loads `BuiltinAppletContent.qml`; they do not exercise or replace the separately compiled Bluetooth applet. The previously coordinated catalog/resolver count, entry, and capability assertions remain the only existing C++ test edits. No collision is observed.

- 2026-08-31T06:35:52-06:00 — Completed the source midpoint: a pure bounded non-address projection/request target, shell-private exact-owner BluetoothClient controller, single discovery-lease close/owner-loss lifecycle, compiled keyboard/accessibility QML, audited manifest/registry/profile/host/package seams, focused pure/controller/offscreen/package test sources, and mutation-sensitive boundary scripts. Both direct CMake policy gates pass with their poison controls, both JSON files parse, and `git diff --check` passes; no configure/build/CTest/private bus was used. The Program Manager explicitly approved only the minimal Bluetooth count/entry/capability additions in shared `tests/applets/tst_catalog.cpp` and `tests/applet_runtime/tst_applet_instance_resolver.cpp`; all other existing test content remains untouched. Shared production edits are additive CMake, built-in registry, QML facade threading, stock-profile placement, and shell composition. No path collision is observed. Dorothy now exclusively owns the serialized lane for S3, so B1 remains on docs/direct-static/provenance work.

- 2026-08-31T06:00:28-06:00 — Claimed QQ-004.14 from exact clean base `ab203cac213b4bff882151de2398b4c1b46c99cb` in isolated branch/worktree `worker/bluetooth-applet-b1` / `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-applet-b1`. Ownership is limited to the new shell-private Bluetooth applet, focused tests, smallest additive manifest/registry/host/package/CMake seams, its primary wiki page, and narrowly reciprocal documentation. The applet will consume only public Bluetooth1/client truth and operations; pairing, trust, keys, BlueZ, Agent1, audio correlation, hardware and manager ledgers stay out of scope. No compiler, CTest, or private-bus resource will be used before explicit release.
