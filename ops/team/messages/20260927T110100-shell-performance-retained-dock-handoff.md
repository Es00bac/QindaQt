# Retained dock and launcher refresh handoff

- Timestamp: 2026-09-27T11:00:57-06:00
- Worker: shell-performance-codex
- Exact candidate: `8414e435456f4ad6f27a03cd8c6fcc0383ad40df`
- Branch: `fix/shell-icon-performance`, pushed to qinda hub
- Base: accepted snapshot candidate `7d2a1601`
- Request: independent exact review, then manager integration and both-machine package/live gates.

## Delivered behavior

TaskList/Quick Launch use a bounded transport-free keyed Qt row model, preserving
surviving delegates across metadata updates, insertion, removal, and reorder.
Only newly inserted dock tiles fade in with the short motion token; reduced
motion is immediate, and input/layout geometry remains unchanged. Preview
selection follows task/member identity across moves and closes on removal.

Launcher persistence no longer clears every dock/recent value during a validated
same-owner post-commit refresh (including successful recent-app writes). Editing
stays disabled until Ready. Owner replacement, including Authenticating to
Authenticating, transport loss, malformed resync, and confirmed empty dock
still revoke presentation. An explicit ownerChanged subscription closes the
same-state owner replacement fence.

## Changed paths

- `src/shell/task_list/applet/`: keyed_row_model header/source/CMake and TaskListApplet/TaskListEntryButton QML.
- `src/shell/desktop_controls/`: CMake, quick_launch_controller.cpp, QuickLaunchApplet.qml, DockItemTile.qml.
- `src/shell/launcher/src/launcher_persistence.cpp`.
- `tests/shell/task_list/`: model test and additive CMake registration.
- `tests/shell/desktop_controls/`: retained-delegate regression, existing drag test's real-centre settle condition, CMake registration.
- `tests/shell/launcher/tst_launcher_persistence.cpp`.
- Wiki TaskList/DesktopControls/Launcher/module-boundaries, ADR-0278 and registrations; own worker/messages records.

## Verification

Native Release build uses isolated `build/performance`, Qt 6.11.1, strict compiler warnings, KWin plugin disabled. Relevant targets compiled successfully (exit 0).

`ctest --test-dir build/performance -R '(launcher-persistence|task-list-(keyed-row-model|applet-dock-qml)|desktop-controls-(offscreen-dock|dock-retained-delegates))$' --output-on-failure`: exit 0, **5/5 CTest rows**, **51 QtTest cases** including init/cleanup.

The keyed model runs **64 delegates over 1,000 metadata publications**, with 64 creations, zero removals, and identical surviving object pointers; subsequent insertion/removal/reorder creates exactly one new delegate. Compiled dock tests prove focus and fade continuity, immediate removals, preview identity and real persistence commit/resync boundaries. Persistence tests cover a recent-app write, confirmed empty, owner replacement without state-enum change, bus loss and malformed reply.

`ctest --test-dir build/performance -R '(task-list-applet-boundary|desktop-controls-boundary|shell-icon-coverage)$' --output-on-failure`: exit 0, **3/3**.

`/home/cabewse/.local/share/mkdocs-venv/bin/mkdocs build --strict`: exit 0.
`tools/validate-docs`: exit 0, **417 documents**. `git diff --check`: exit 0.

## Bounded caveats and help offer

Root owns integrated broad native/profile/nested validation, overlay/package
updates, both-machine installation and aged-session live proof. This worker
did not touch either physical session. Group identity follows the first stored
application because the persisted dock schema has no UUID; replacing that
member may replace the affected group, documented in ADR-0278. Removal is
immediate to retire actions safely; no disappearing-target geometry animation.

Read the shell workgroup queue and current peer threads after handoff; its
older assignments do not override the active manager request. Available for
exact review repairs or a bounded reproduction of any combined-tree dock,
QML-import, persistence, or profile regression while root packages/integrates.
