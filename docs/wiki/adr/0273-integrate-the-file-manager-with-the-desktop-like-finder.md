# ADR-0273: Integrate the File Manager with the desktop like Finder

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** First-party applications (File Manager) and Shell (desktop surface)
- **Amends:** [ADR-0262](0262-applications-is-browsed-in-the-file-managers-ordinary-views.md)
  decision 7's deferral of Keep in Dock and of dragging an application to the
  dock, and decision 6's "application rows never enter a drag".
- **Supersedes:** None
- **Superseded by:** None

## Context

On macOS the file manager is part of the desktop. Other applications' "Show
in folder" opens a Finder window with the file selected, the desktop's icons
answer to Finder's menus, and an application can be kept in the Dock from
where you find it. QindaQt's File Manager had none of this:

- Browsers, editors and download managers call the freedesktop.org
  `org.freedesktop.FileManager1` interface (`ShowItems`, `ShowFolders`,
  `ShowItemProperties`) on the session bus. Nothing in a QindaQt session
  answered it, so "Show in folder" fell back to the caller's own guess, or
  started whichever other file manager was installed. On the workstation
  that is Dolphin, through `org.kde.dolphin.FileManager1.service`. File
  Manager itself had no D-Bus authority and no way to open a folder with an
  item selected.
- The desktop's icon menu said Open, Cut, Copy, Rename… and Delete (which
  moved to Trash). Its background menus had their own words for New Folder
  ("Create Folder…") and no New File, Get Info or Open With. The desktop
  surface may link only File Manager's isolated local-files boundary and,
  since ADR-0260, its values-only menu catalog.
- ADR-0262 left Keep in Dock and dragging an application to the dock out
  until the dock had a public boundary. W13 (ADR-0265) published one: the
  `Settings1DockPins` helper and the `application/x-qindaqt-desktop-entry-id`
  drag format.
- A File Manager process owns exactly one window, and W10 (ADR-0269) makes
  File Manager the one authority for Open With (handlers from the shared
  MIME associations) and New File (templates).

## Decision

1. **File Manager serves `org.freedesktop.FileManager1`** at
   `/org/freedesktop/FileManager1`. Every File Manager process queues for
   the name and none replaces an owner, so the first one serves and the next
   takes over when it exits. A workspace picker and the `--check-*` probes
   serve nothing.
2. **Every URI is checked before anything is shown** (`planReveal`). A URI
   must be a `file:` URI of an absolute local path, with no host other than
   `localhost` and no user, port, query or fragment. A folder must resolve
   once to a readable, enterable directory, the rule
   `FileBoundary::openLocalFolder` already applies. An item must exist in
   such a folder, and a symbolic link is the item itself. One bad URI refuses
   the whole call with `InvalidArgs`, so the caller falls back rather than
   seeing half of its request. A call may carry 256 URIs and open 8 windows.
   Items group into one window per folder. A request no window could show
   answers `Failed`. The caller's `StartupId` activates only the first
   window of a call.
3. **Which window.** The serving process shows a request in its own window
   when that window already shows the folder or has shown nothing yet. Every
   other request starts one more File Manager process. File Manager is not
   made multi-window for this.
4. **One reveal command line**, defined once by
   `FileBoundary::revealArguments`: one `--select=<name>` per entry, then
   `--action=<id>` when an action follows, then the folder as the single
   positional argument. The action may only be `file.properties` (Get
   Info), `file.open-with` or `file.new-file`. The boundary refuses any
   other, and File Manager ignores it. File Manager selects the entries,
   scrolls to them, and activates the action through its coordinator, as
   the menu item would. `--service` starts File Manager with its window
   hidden for D-Bus activation. The `FileManager` install component carries
   `org.qindaqt.FileManager.FileManager1.service`, named after File Manager
   so that it can sit beside another provider of the interface.
5. **The desktop's menus are File Manager menus.** Each entry backed by a
   File Manager action takes its label from File Manager's public menu
   catalog, by action id, through the desktop surface's `DesktopFileActions`
   QML singleton. The icon menu is Open, Open With… (files only), Get Info,
   Rename, Cut, Copy and Move to Trash, plus the dock's Add to Dock.
   Every background style gains New File…, Select All and Get Info for the
   Desktop folder, and Refresh now re-reads the folder instead of
   rearranging. Desktop-only entries (Arrange, Sort By, Clean Up, settings,
   Applications, Open Terminal Here) stay. Both menus build their rows from
   descriptors with one shared row component.
6. **The desktop hands File Manager's dialogs to File Manager.** Get Info,
   Open With and New File start File Manager on the Desktop folder with the
   icon selected (`FileBoundary::revealLocalItem`, fenced by the listed
   identity) or with nothing selected (`FileBoundary::runLocalFolderAction`),
   and File Manager runs the action there. The shell gains no MIME, handler,
   template or D-Bus dependency. The desktop re-lists its folder shortly
   after it changes on disk, so a file made by New File, a download or any
   other program appears without a manual refresh.
7. **Keep in Dock** is a checkable File Manager action,
   `application.keep-in-dock`, offered for the one selected application in
   the Applications place. It writes through `Settings1DockPins`, and its
   check shows only what Settings1 has confirmed. Dragging one application
   row offers its desktop-entry id under
   `application/x-qindaqt-desktop-entry-id`, which the dock keeps. Nothing
   in File Manager accepts that format, and application rows still never
   enter a file drag.

## Consequences

- File Manager has one D-Bus authority. It only validates paths and shows
  windows. Show in folder works from Chromium and Electron applications,
  Firefox and KDE applications while any File Manager window is open, and
  through activation otherwise.
- With another provider installed, the bus chooses which one it starts when
  no File Manager process is running. On a host with Dolphin, that may be
  Dolphin. Making the session prefer File Manager's activation file is left
  to the session.
- A request for a folder no window shows opens a new window, not a tab. The
  desktop's Get Info, Open With and New File open a File Manager window on
  the Desktop folder, because the dialogs belong to File Manager.
- The desktop surface links File Manager's values-only catalog and its
  boundary only. Its menus show whatever the catalog says. A catalog id that
  disappears would leave an empty row; the `qindaqt.desktop-file-manager-menus`
  row fails on one.
- The Desktop folder is watched with `QFileSystemWatcher`, one watch per
  output's surface. A burst of changes becomes one re-list, and an unchanged
  listing keeps its rows.
- Tests: `planReveal` over real trees; the service over a private
  `dbus-run-session` bus without activation directories, with a recording
  window factory, and the production factory's reuse rule; the boundary's
  reveal command line and refusals; Keep in Dock against a scripted
  Settings1; the desktop controller's actions and folder watching; the
  desktop menus' words and routing offscreen with a recording
  `qindaqt-file-manager` on `PATH`; and the installed activation file.
  Showing a real window from a real browser remains a live check.

## Revisit when

- File Manager holds several windows or tabs in one process (W16 tabs): show
  a request in a new tab or an existing window instead of a new process.
- The session decides which `org.freedesktop.FileManager1` provider it
  prefers.
- File Manager's boundary gains a light Open With candidates query the shell
  may use: then the desktop could offer an inline Open With submenu.
- The desktop surface takes keyboard focus: then its menus can show the
  catalog's shortcuts.
