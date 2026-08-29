# Settings Center S1 exact candidate review: FAIL

- Reviewer: Noether the 4th
- Timestamp: 2026-08-28T15:50:46-06:00
- Verdict: **FAIL**
- P0/P1/P2/P3: **0/1/2/0**
- Exact candidate: `7e6f133e280920f98fcb0ea79385d496b7871bd6`
- Exact tree: `3d0d25fd1ac995e94fafd4ba9d401db6970b6d95`
- Exact parent: `0760e08e1118d6a8b8101f6d17d271d1b766cc96`
- Detached review worktree: `/mnt/d/QindaQt/reviews/settings-s1-noether`
- Candidate state after every gate: byte-clean

## Blocking findings

### P1-1 — installed-package evidence borrows uninstalled build QML

`src/apps/settings_center/main.cpp:39-50` adds the compiled build QML root for
any executable located beneath the build root. The supposedly relocated stage
is required by `tests/apps/settings_center/check_installed_routes.cmake:11-17`
to live beneath that same root. The installed row therefore cannot establish a
component-only package boundary. Removing the complete staged Appearance QML
module leaves the in-root installed executable resident until timeout 124;
the same install at a sibling prefix outside the compiled root exits 3. Exact
setup and artifact paths are in material finding `1787953783`.

Repair must recognize only the real build executable location, not arbitrary
descendants. A mutation-sensitive installed test must withhold one required
staged module while the build tree remains present and require construction
failure, then restore/reinstall the complete component and prove both routes
using only it.

### P2-1 — unavailable PageTabs omit the registered reason

`src/apps/settings_center/SettingsSidebar.qml:59-69`,
`SettingsNavButton.qml:28-29`, and
`SettingsCompactHeader.qml:59-68` expose only the ordinary route description
when `available == false`; neither wide nor compact PageTab receives
`modelData.unavailableReason`. The review descriptor's registered reason is
`Subsystem daemon crashed`, but the actual wide accessible description is
`Unavailable. Unavailable hardware`. This contradicts the owning page's
promise that unavailable tabs describe their unavailability.

Repair both presentations and assert the exact registered reason through Qt's
accessible interface in a fully token-published compiled page test.

### P2-2 — Escape cannot return focus to an active unavailable tab

`SettingsSidebar.qml:21-28` and `SettingsCompactHeader.qml:22-29` call
`forceActiveFocus()` on the active PageTab. Their delegates correctly set the
Controls `available` property false, making the inherited control disabled;
that disabled PageTab cannot accept active focus. The review assertion remains
false after 500 ms. Main's Escape shortcut therefore cannot meet its documented
focus-return contract while the fail-closed unavailable route is active.

Repair wide and compact focus-return behavior without making unavailable route
activation possible, and pin Escape/focus truth for the valid unavailable
descriptor in the compiled offscreen test.

The exact external QML reproduction and command for both P2s are in midpoint
`1787953446`; it intentionally omits token publication, so its expected token
warnings are not used as findings. Candidate-owned warning cleanliness was
verified separately under complete QST publication.

## Passing evidence retained

- Fresh strict Debug build of Settings and all three S1 binaries: PASS.
- Fresh strict Debug exact selector: 9/9 PASS.
- Fresh strict Release build and exact selector: 9/9 PASS.
- Direct Debug registry/controller/page: 9/9, 10/10, and 6/6 PASS.
- Direct page run under `QT_FATAL_WARNINGS=1`: 6/6 PASS, zero warning abort.
- Adjacent Debug Appearance selector: 4/4 PASS.
- Additional canonical/alias/deep-link hostile values: 15/15 reject with exit
  2 and exact diagnostics; a repeated option with a hostile final value also
  rejects with exit 2.
- `git diff --check`: PASS.
- `./tools/validate-docs`: PASS, 92 Markdown documents and MkDocs navigation.
- `./tools/check-source-shape`: PASS, 1,357 source files and zero skips.
- strict MkDocs build to
  `/mnt/d/QindaQt/builds/settings-s1-noether/docs`: PASS.
- Exact commit/tree/parent objects and author metadata were independently read;
  changed paths contain no `ops/team`, manager ledgers, or generated artifacts.
- Final detached candidate `git status --short`: empty.

Passing ordinary tests do not waive the three mutation-sensitive failures.
The exact original candidate must not be integrated.

## Requested next action

Ada the 3rd retains
`/mnt/d/QindaQt/worktrees/settings-center-navigation-s1-sylvie` and has received
all exact reproductions. She should commit one clean non-amended descendant
closing P1-1, P2-1, and P2-2 plus focused Debug/Release/package/docs evidence,
then request this same reviewer's exact rereview. Noether the 4th remains
available for that rereview.
