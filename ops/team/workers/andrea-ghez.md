---
name: Andrea Ghez
role: First-party menu-export implementer
provider: Z.AI GLM
model: zai-coding-plan/glm-5.3
reasoning: high
status: handoff
feature: first-party global-menu export for Terminal and Text Editor (P1-01 bounded repair)
worktree: /home/cabewse/work_SPaC3/container-wm-workers/first-party-menu-export
started_at: 2026-09-04T17:13:00-06:00
updated_at: 2026-09-04T19:33:00-06:00
---

# Andrea Ghez

- Role: first-party menu-export implementer (Shell delivery queue).
- Provider/model: Z.AI GLM `zai-coding-plan/glm-5.3`, reasoning high.
- Status: handoff — exact candidate
  `ba88f0b153a762ff4ab604bc5c2157a44a3cc296`
  (tree `53c06fc095ab74af157912df1c9e71902c47b98d`): bounded one-round repair
  of the Blackburn P1-01 rejection — `ApplicationMenuExport::eventFilter()`
  now handles the `QEvent::PlatformSurface` lifecycle (synchronous withdraw
  on `SurfaceAboutToBeDestroyed`, queued republish of the freshly obtained
  identity on `SurfaceCreated`, fail-closed `publishIdentity()` while the
  surface is dead, close/quit teardown and dbusmenu revision behavior
  unchanged); one new registered private-bus row with three recreation rows,
  both defect detectors verified to fail on the unrepaired `e8e5170b`
  exporter; 90/90 selector rows green in Debug and Release; static gates
  green. Elizabeth Blackburn recheck requested, then manager integration.
- Exact base: `23b549db` (product base is the reviewed candidate
  `e8e5170beb9849752341d1fec1ad47ffa5e6ca1e`).
- Branch: `worker/first-party-menu-export`.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/first-party-menu-export`.
- Product authority: `src/apps/terminal/**`, `src/apps/text_editor/**`,
  `src/app_shell/menu_export/**`, `tests/apps/terminal/**`,
  `tests/apps/text_editor/**`, `tests/app_shell/**`, and the touched wiki
  pages. Two bounded deviations from the owned-path list in the earlier
  candidate round, both directed by the lane text ("route all three apps
  through it"): `src/apps/file_manager/**` delegates
  `composeFileManagerMenuExport` to the shared entry, and
  `tests/app_shell/CMakeLists.txt` gained its own
  `find_program(dbus-run-session REQUIRED)` for fresh build trees.

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
- 2026-09-04T19:05:00-06:00 — claim: bounded repair round after the Blackburn
  REJECT (P1-01 stale window identity on surface recreation); read the
  verdict and its standalone reproduction, then repaired the shared exporter
  at the cause.
- 2026-09-04T19:20:00-06:00 — material finding: republishing directly from
  the `SurfaceCreated` handler would recurse, because an X11 `winId()` read
  inside `publishIdentity()` can itself create the surface and deliver
  `SurfaceCreated` synchronously; the republish therefore runs on a queued
  turn and rechecks started/surface/identity/owner there. The absent-registrar
  recreation control needs the registrar fixture to release only its
  well-known name (keeping the object registered) or the export's late
  `UnregisterWindow` is unobservable.
- 2026-09-04T19:33:00-06:00 — handoff: candidate
  `ba88f0b153a762ff4ab604bc5c2157a44a3cc296`; both new defect-detector rows
  verified failing on the unrepaired exporter; selector 90/90 in Debug and
  Release; static gates green; requested Elizabeth Blackburn recheck then
  manager integration.
