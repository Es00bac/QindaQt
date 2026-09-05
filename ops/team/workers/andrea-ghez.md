---
name: Andrea Ghez
role: First-party menu-export implementer
provider: Z.AI GLM
model: zai-coding-plan/glm-5.3
reasoning: high
status: handoff
feature: first-party global-menu export for Terminal and Text Editor
worktree: /home/cabewse/work_SPaC3/container-wm-workers/first-party-menu-export
started_at: 2026-09-04T17:13:00-06:00
updated_at: 2026-09-04T18:51:29-06:00
---

# Andrea Ghez

- Role: first-party menu-export implementer (Shell delivery queue).
- Provider/model: Z.AI GLM `zai-coding-plan/glm-5.3`, reasoning high.
- Status: handoff — exact candidate `e8e5170beb9849752341d1fec1ad47ffa5e6ca1e`
  (tree `3c79b361dfbcdd0e94a6d5f193d0bec1179ca2bb`): Terminal and Text
  Editor compose the first-party AppShell global-menu export through one new
  shared `composeFirstPartyMenuExport` entry that the File Manager now also
  uses; 8 new private-bus/policy rows registered; 89/89 selector rows green in
  Debug and Release; docs updated. Independent exact review then manager
  integration requested.
- Exact base: `86ad7c3854a5e35efb0a3b3e65444c432448fa0e`.
- Branch: `worker/first-party-menu-export`.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/first-party-menu-export`.
- Product authority: `src/apps/terminal/**`, `src/apps/text_editor/**`,
  `src/app_shell/menu_export/**` (additive helper), `tests/apps/terminal/**`,
  `tests/apps/text_editor/**`, `tests/app_shell/**`, and the touched wiki
  pages. Two bounded deviations from the owned-path list, both directed by the
  lane text ("route all three apps through it"): `src/apps/file_manager/**`
  now delegates `composeFileManagerMenuExport` to the shared entry (behavior
  and env-var seam preserved; the File Manager row was rerun green), and
  `tests/app_shell/CMakeLists.txt` gained its own `find_program(dbus-run-session
  REQUIRED)` to fix a pre-existing empty-command registration on fresh build
  trees.

## Updates

- 2026-09-04T17:13:00-06:00 — claim: compose the AppShell menu export in the
  Terminal and Text Editor mirroring the accepted File Manager pattern
  (a8c171c, ADR-0068); read the global-menu wiki contract, the AppShell
  menu-export module, and the File Manager composition/tests.
- 2026-09-04T17:58:00-06:00 — material finding: the AppShell source policy
  forbids `QDBusConnection::sessionBus()` inside `src/app_shell/menu_export`,
  so the shared helper takes the bus injected and each executable resolves its
  own session bus; the File Manager's per-app test seam moved into the shared
  helper under a BUILD_TESTING-only compile definition so all three apps share
  one composition path.
- 2026-09-04T18:20:00-06:00 — material finding: the real Terminal process
  aborts under `QT_FATAL_WARNINGS=1` because qtermwidget under the offscreen
  QPA emits "This plugin does not support propagateSizeHints()"; the Terminal
  rows therefore run without fatal warnings (documented in the test, matching
  the existing qtermwidget-linked rows) while the Editor rows keep them.
- 2026-09-04T18:47:00-06:00 — material finding: `tests/app_shell` registered
  `qindaqt.app-shell-menu-export-private-bus` with an empty command on fresh
  build trees because only `tests/session` (a later directory) defined
  `QINDAQT_DBUS_RUN_SESSION`; fixed inside my owned tests/app_shell path.
- 2026-09-04T18:51:29-06:00 — handoff: candidate
  `e8e5170beb9849752341d1fec1ad47ffa5e6ca1e`; selector 89/89 in Debug and
  Release; static gates green; requested independent exact review then
  manager integration.
