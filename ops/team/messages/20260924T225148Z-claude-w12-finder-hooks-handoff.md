# claude-w12-finder-hooks handoff

- Time: 2026-09-24T22:51:48Z
- Outcome: W12 — Finder-style integration hooks (ADR-0273): org.freedesktop.FileManager1 in the File Manager,
  the desktop's menus as File Manager menus (catalog words; Get Info, Open With, New File handed to File Manager
  through its public boundary), and Keep in Dock / drag to dock from the File Manager's Applications place.
- Branch: worker/claude-w12-finder-hooks-20260923 on the hub, last code commit d5332307 (the branch head is the commit that adds this record), based on the round head 2d896244 (W13 included).
- Gates (code-first round, nothing built): configure exit 0; syntax-check 19 ok, 9 NEEDS-GENERATED
  (test .moc includes; each re-checked clean with the include stripped), 0 FAIL; ./tools/validate-docs OK.
- Final build/test run should look first at: qindaqt.file-manager-file-manager1 (private dbus-run-session bus),
  qindaqt.desktop-file-manager-menus (offscreen menus + recording qindaqt-file-manager on PATH),
  qindaqt.file-manager-application-dock-pins, qindaqt.desktop-surface-contents (folder watcher),
  qindaqt.file-manager-reveal-request, qindaqt.file-manager-desktop-file-boundary, qindaqt.file-manager-action-catalog,
  qindaqt.desktop-surface-offscreen-qml (style table), qindaqt.desktop-icon-interactions,
  qindaqt.file-manager-installed-runtime (activation file), then ctest -L "file-manager|desktop-surface".
- Package note: the FileManager component installs share/dbus-1/services/org.qindaqt.FileManager.FileManager1.service
  (Exec=<bindir>/qindaqt-file-manager --service); the ebuild's cmake install picks it up. Hosts with Dolphin also
  have org.kde.dolphin.FileManager1.service for the same name; the bus picks one when no File Manager runs.
- Caveats:
  - Depends on W10 (ADR-0269), which is not in the round yet: the desktop menus take the labels of file.open,
    file.open-with and file.new-file (and file.properties' "Get Info") from the File Manager catalog, and
    reveals may run file.open-with / file.new-file. Until W10 merges, those desktop rows have empty labels and
    qindaqt.desktop-file-manager-menus fails. Keep in Dock's catalog order (7) should follow W10's renumbering
    (same order as Show Desktop Entry File; the catalog row asserts equality).
  - Files both W12 and W10's working tree change: src/apps/file_manager/{CMakeLists.txt, main.cpp,
    public/file_manager_menu_catalog.cpp, ui/ApplicationsPlaceActions.qml, ui/FileContextMenu.qml, ui/Main.qml},
    tests/apps/file_manager/{CMakeLists.txt, tst_file_manager_action_catalog.cpp}, docs/wiki/apps/file-manager.md,
    docs/wiki/adr/index.md, mkdocs.yml. W10 moves main.cpp's UI-contract list to runtime/ui_contract_probe.cpp:
    "entryReveal" must move with it.
  - Live checks not done: Show in folder from a real browser, D-Bus activation on a real session (with
    Dolphin installed the bus may start Dolphin when no File Manager runs), dragging an application from File
    Manager onto the dock across processes, and the desktop watcher on a real Desktop folder.
  - src/apps/file_manager/ui/Main.qml was already over the QML size limit (390 > 350 non-blank) before W12;
    W12 adds 12 lines. tests/shell/desktop_surface/tst_desktop_surface_qml.cpp is at 599 of 600.
- Next action: manager merge into the round.
