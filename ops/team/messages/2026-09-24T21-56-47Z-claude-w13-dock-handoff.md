# Handoff: W13 dock drag and drop, groups, and pinning (claude-w13-dock)

- Branch `worker/claude-w13-dock-20260923`, base `a77e2c1a` (head of
  `round/code-first-20260924`); the candidate is the commit that adds this
  message. Code-first round: configured and syntax-checked, not built.
- **Contracts that landed (ADR-0265):**
  - `panels.dockItems` (schema v2 object, default `{}`): `{"version": 1,
    "items": [...]}` of application, folder, file, one-level group, and Trash
    records; 32 items, 64 applications, 16 per group, names ≤ 64, paths
    ≤ 1,024; whole-value rejection. One codec: `src/services/dock_items`
    (`QindaQt::DockItems`), plus `Settings1DockPins` (`QindaQt::DockPins`)
    for other processes to pin with confirmed readback.
  - One-time migration from `panels.launcherPinned` (never written again).
    The launcher's `pinned()` is the dock's applications; every pin/unpin/move
    is a whole-dock commit under ADR-0012 semantics. Launcher pinned ceiling
    16 → 64.
  - Dock UI (quick-launch applet): tiles for every kind, drops with a live
    gap (launcher rows as `application/x-qindaqt-desktop-entry-id`, uri-lists,
    `.desktop` files → their application, the Trash folder → Trash), drag to
    move/group/remove, group and folder stacks that unfold from the dock
    (ControlPopupFrame/PanelPopup), prompts for New Group/Rename/Empty Trash,
    menus repeating every gesture. Opening only through the Places folder
    opener and the File Manager `FileBoundary`.
  - One icon per app: quick-launch gains `windows.read`/`windows.activate`;
    running dot plus "Running" in the accessible description; a dock zone's
    task strip omits windows a top-level pinned tile stands for.
  - Pin/Keep in Dock from launcher rows (menu and drag), start menu program
    list, running task tiles, and desktop icons (Add to/Pin to Dock).
- **Checks run:** `cmake --preset dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
  (exit 0, re-run after each CMake edit); `syntax-check.sh` on the branch:
  33 ok, 10 NEEDS-GENERATED (test `.moc` includes only), 0 FAIL; those 10
  test files re-checked with the `.moc` include stripped: ok, no warnings;
  every other translation unit that includes a changed header: ok;
  `./tools/validate-docs`: 397 documents OK; `node --check` and a small node
  sanity run of `DockDropGeometry.js`. No build, no ctest.
- **Final build: look first at** the new `qindaqt_dock_items`/`qindaqt_dock_pins`
  targets and `qindaqt.services-dock-items`/`-dock-pins`; the launcher rows
  (`launcher-persistence`, `launcher-settings-contract` — real service storing
  the nested value, `launcher-qml`, `launcher-controller`, the standalone
  launcher test route now adding `src/services/dock_items`);
  `desktop-controls-dock`, `desktop-controls-offscreen-dock`,
  `desktop-controls-offscreen-menus`, `desktop-controls-boundary`,
  `desktop-controls-composition`; `panel-geometry-offscreen`; the task-list,
  start-menu, and desktop-surface QML rows; the applet resolver row.
- **Caveats:** live-only: real pointer drags off the panel, cross-window drops
  from the File Manager, Wayland stack placement, real window-to-app
  matching. Desktop icons cannot be dragged onto the dock (their drag is
  internal); their menu adds them. Drops land on the pins strip (and within a
  tile of an empty dock), not on the task part of a dock. No dedicated
  offscreen tests for the launcher/start-menu/task-list/desktop menu entries
  (their facade calls are covered in C++). Source shape: `QuickLaunchApplet.qml`
  is 325 non-blank lines (review threshold; split into six collaborators);
  `TaskListApplet.qml`, `TaskListEntryButton.qml`, and
  `tst_applet_instance_resolver.cpp` were already over their limits and grew
  by 14, 20, and 9 lines.
- **Next:** manager merge into the round branch. Lane C's W12 wires the File
  Manager's Applications "Keep in Dock" through `Settings1DockPins` and can
  offer the desktop-entry drag format. W14 (applet dragging) should keep the
  dock strip's DragHandler/DropArea out of edit mode's way.
