# PF11 candidate handoff — Codex

2026-09-29T21:28:00-06:00

## Candidate commits

- Fork `qindaqt-kwin`: `6c8c397451` (`feature/plasma-free-compositor-deps`), exact base `79fa351f6e823d302996453e12356a674e4b64ca`, worktree `/home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/plasma-free-compositor-deps`.
- Consumer `container-wm`: `fff921ab` (`feature/plasma-free-compositor-deps`), exact base `a33d33c430c71ac63062d1def44c61e7a3c00ce5`, worktree `/home/cabewse/work_SPaC3/container-wm.worktrees/plasma-free-compositor-deps`.

Both commits are pushed to their qinda hubs. The fork removes production discovery of Plasma Activities, libplasma, Breeze, and Aurorae; fixes Activities capability false; removes the Plasma output locator and bundled thumbnail-grid; and retains QindaQt decoration, switcher ABI, and QtQuick-based outline/notification/effect frames. The consumer replaces PlasmaCore.Dialog in its QindaQt switcher with QtQuick.Window and tests real QindaQt module metadata. ADR-0311 supersedes ADR-0309; compositor-session wiki and navigation are updated. Root owns source pin/shared integration.

## Verification

- Fork configured with all four retired CMake package finds disabled; production build passed (1,428 initial actions; 27 final incremental actions; subsequent no-op rebuild exit 0). Private install used explicit `DESTDIR`.
- Install manifest contains 455 entries; all entries exist, 453 staged files. The staged fork exports `QINDAQT_KWIN_BUILD_ACTIVITIES FALSE`; no outputlocator or thumbnail_grid installed; five installed QML files have no Plasma imports. Collision check passed against stock compositor/decoration manifests.
- Fork staged library direct and recursive dependency scans found no Plasma, Breeze, Aurorae, or Milou dependency.
- Consumer affected-target build against the exact fork stage passed (1,130 actions earlier; final affected-target pass reported no work, exit 0). Switcher contract/QML lint passed. Focused CTest command selected 14 decoration, switcher, group/context-menu, shell/workspace cases: 14/14 passed with fork build `bin` and staged `lib64` on `LD_LIBRARY_PATH`.
- Strict MkDocs build exited 0; `python3 tools/validate-docs` validated 456 Markdown documents/navigation; both `git diff --check` checks passed.

## Bounded caveat

A final whole-consumer build stopped at 7,135/7,997 while linking unrelated `qindaqt_kwin_input_adapter_tests`: existing `kwininteractionfilter*` references to `MinimizedGatherPagerRouter::{touchActive,touchUp,active,pointerActive,cancel}` were unresolved. No PF11-owned code participates in that target. Affected compositor/decoration/switcher targets build successfully. The initial non-DESTDIR private install attempt failed while creating `/usr/libexec/qindaqt-kwin`; the preserved log records the attempt. Inspection found that named target absent. Recovery installed only into the ignored private DESTDIR stage; no external write was observed.

Requested next action: independent review of both exact commits, then root-managed integration and the combined program build.
