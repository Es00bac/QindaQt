# claude-w10-file-actions handoff — W10 File manager basics, part 1 (the right-click set)

- Candidate: `4b170c31` on `worker/claude-w10-file-actions-20260923` (hub), worktree
  `/home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w10-file-actions`.
  Base `58871707`; the round head `6151fa24` (W12 + W14) is merged in.
- ADR: 0269 (Open With widens ADR-0029's launch contract; KArchive dependency).
- Round rules: code-first — configure plus syntax checks only; nothing built or run.

## What landed

- Item menu: Open, Open With ▸ (recommended apps for the type and its parent types, default
  marked; Other Application… chooser over the Applications place rows; Always Open With),
  Open in New Window, Cut/Copy, Copy Path, Rename, Duplicate, Make Link, Copy To/Move To,
  Compress, Extract (archives), Add to Sidebar, Get Info, Move to Trash, Delete Permanently
  (Shift+Delete, always confirmed), Put Back (in Trash). Background menu: New Folder, New File ▸
  (empty + ~/Templates), Paste, Open Terminal Here (qqterm --working-directory), Select All,
  Sort By ▸, View ▸, Show Hidden, Refresh, Get Info for the folder. Same commands in File/Edit/View
  menus via the shared public catalog (file.open replaces application.open; Properties → Get Info).
- Open With: shared Exec grammar hands local files to %f/%F/%u/%U (whole args); OpenWithLauncher
  (no shell; terminal via qqterm -e; D-Bus entries refused; ≤ 32 files). Associations only through
  Settings' DefaultApplicationsStore (new per-MIME API + shared composition helper).
- Mutation pipeline: new kinds CreateFile, Link, Delete, Compress, Extract; KArchive behind
  ArchiveCodec (zip out; zip/tar[.gz/.bz2/.xz/.zst] in; safe names, links last, bounded,
  cancellable, partial output removed).
- Found and fixed: batches into one folder failed after the first item (folder time stamp);
  batches now accept their own writes (same device/inode) — this also fixes multi-item paste.

## Gates

- `cmake --preset dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`: exit 0.
- `syntax-check.sh <worktree>` (vs round): 40 ok, 10 NEEDS-GENERATED, 0 FAIL, 0 SKIP. Each
  NEEDS-GENERATED file (moc only) also compiles whole with its moc include dropped.
- All 52 translation units that include a changed header compile (same deep check).
- `tools/validate-docs`: 402 documents green.

## For the final build/test run, look first at

1. `qindaqt.file-manager-file-actions-ui` (new; QT_FATAL_WARNINGS; context-menu sub-menu
   entries are named in onAboutToShow) and `qindaqt.file-manager-browsing-ui` /
   `-applications-ui` (menu changes: View ▸ replaced the view toggle; Open is contextOpenAction).
2. `qindaqt.file-manager-file-actions-mutation` (real KArchive: tar.gz fixture, hostile zip name).
3. `qindaqt.file-manager-open-with` (needs shared-mime-info for .c/.txt/.png types).
4. `qindaqt.settings-default-apps-mime-types`, `qindaqt.launcher-execution`,
   `application-catalog.launch-support`, `qindaqt.file-manager-action-catalog` (52 actions,
   unique shortcuts), and W12's `tst_desktop_file_manager_menus` (catalog labels).

## Caveats

- Packaging: ebuild RDEPEND needs `kde-frameworks/karchive:6` (not edited). CMake requires KF6Archive.
- Live checks not possible here: Open With launching real apps, qqterm `--working-directory` in a
  running QQ_Term (single-instance forwarding untested), zstd tarballs (depend on KArchive's build).
- Source shape: FileContextMenu.qml 326 non-blank (review threshold 275, max 350) after moving
  its sub-menus into Context*.qml; Main.qml 421 and tests/apps/file_manager/CMakeLists.txt 609
  were already over their limits at the round head (402 and 606); W10's rows moved to
  FileActionsTests.cmake.
- Open With cannot use D-Bus-activatable apps yet; Compress writes zip only; Put Back and
  Delete-in-Trash cover the home Trash only (per-volume Trash is S4).
