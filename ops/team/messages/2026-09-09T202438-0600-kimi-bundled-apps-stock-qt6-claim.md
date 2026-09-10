# Bundled apps on stock Qt 6 — claim

- Worker: kimi-bundled-apps (Moonshot Kimi CLI)
- Status: working
- Base: `main` at `150d37cc` plus preserved shared-checkout changes (untracked Calendar app, compositor work owned by the separate container program — untouched by this lane)
- Date: 2026-09-09T20:24:38-06:00

## Outcome

Calendar, Terminal, Text Editor, and File Manager become first-class bundled
applications on stock Qt 6, comparable to MATE/GNOME/KDE/XFCE bundled apps.
The custom token UI (QST-1/`QindaQt.Controls`) remains only on system surfaces
(shell, panels, Settings, Settings Center, Welcome).

## Decision record

ADR-0116 (`docs/wiki/adr/0116-build-bundled-applications-on-stock-qt6.md`),
accepted by the product owner 2026-09-09, supersedes the bundled-application
scope of ADR-0013/0027/0080/0109. `coding-practices.md`,
`module-boundaries.md`, and `shell/controls.md` are re-scoped in the same
change.

## Lanes (convert presentation first, then finish features)

1. Calendar: stabilize and commit the untracked app (it blocked a clean tree),
   convert 7 QML files to stock QtQuick.Controls, implement real event editing
   and a details pane, register the 3 orphaned test sources, add the wiki page
   and Gentoo packaging (ADR-0096 component + kf6calendarcore).
2. File Manager: convert 17 QML files, then clipboard cut/copy/paste,
   drag-and-drop, properties dialog, bounded recursive search (the four open
   outcomes in TASK_LIST).
3. Text Editor: remove token→palette/QSS chrome, add printing and
   crash-recovery autosave.
4. Terminal: remove custom QSS chrome (keeping ADR-0112 ANSI palettes), bounded
   session restore (profile + cwd), OSC-8 hyperlinks if qtermwidget 2.4 allows.

## Verification

Per-lane: targeted build + `ctest -R '^qindaqt\.<app>'` all green.
Close-out: full Debug build and ctest, `mkdocs build --strict`,
`tools/docs_validation.py`, source-shape checker. HANDOFF/TASK_LIST updated
with exact commits and remaining limits. Container/compositor work owned by
the separate active program is not touched.
