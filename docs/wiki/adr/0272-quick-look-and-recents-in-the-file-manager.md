# ADR-0272: Quick Look and a Recents place read existing authorities

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** First-party applications (File Manager)
- **Supersedes:** None
- **Superseded by:** None

## Context

Jarrod asked for the File Manager's everyday Finder features: Quick Look
(Space previews the selection), type-to-select in every view, and a Recents
place. Tabs, the other feature in the same request, belong to the File Manager
styles work (ADR-0271) and are not part of this decision.

Type-to-select already existed in every view: the four views of ADR-0270
share `ViewportNavigation`, which selects the first entry whose name starts
with what was typed. Its Space key, however, reselected the current entry, so
a name with a space in it ("my notes") could not be typed, and Space was not
free for Quick Look.

The File Manager already decodes one large bounded preview (the Gallery and
Columns views' `gallery-previews` pipeline, ADR-0270) and has the entry facts
pane beside it. The desktop's icons hand dialogs to the File Manager through
`FileBoundary::revealLocalItem` and a short allowlist of reveal actions
(ADR-0273), and the desktop needs Quick Look on its icons too.

A Recents place needs a history of used files. This desktop already has one:
the freedesktop desktop-bookmark file `$XDG_DATA_HOME/recently-used.xbel`,
which GTK's `GtkRecentManager` and KDE's `KRecentDocument` both write (on
qinda it holds what Firefox, Dolphin and KDE applications opened). A second
list kept by the File Manager would miss everything opened elsewhere and
disagree with the file dialogs.

## Decision

1. **Quick Look is one catalog command, `file.quick-look`** ("Quick Look",
   File menu, Ctrl+Y as Finder's Cmd+Y), in the shared public catalog
   (ADR-0260). It is enabled whenever something is selected. Space reaches
   it only through the views' keyboard handling (`ViewportNavigation`
   emits `quickLookRequested`, each view activates the action), never as a
   menu shortcut, because a Space shortcut would take the key from every
   text field. The right-click menu offers it for a selection.
2. **The preview is a `Tk.Popover`** (`QuickLook.qml`) centred over the
   window: a `Tk.Thumbnail` of the entry's icon, covered by the existing
   `gallery-previews` picture once one decodes, beside `EntryFactsPane`.
   Nothing is requested while it is closed. Space or Escape closes it; the
   arrow keys step through the folder by moving the window's
   `EntrySelection`, so the previewed entry is always the selected one, and
   the active view scrolls to it through `FolderViewStack.activeView`.
   Navigating away closes it.
3. **Type-to-select keeps spaces.** While a name is being typed (the one
   second prefix is non-empty), Space is part of the name; otherwise plain
   Space is Quick Look (selecting the focused entry first when nothing is
   selected), and Ctrl+Space still toggles the focused entry's selection.
4. **The desktop calls Quick Look through the File Manager boundary.**
   `file.quick-look` joins `isRevealAction()`, so
   `FileBoundary::revealLocalItem(path, identity, "file.quick-look")` (and
   `DesktopContentsController::runFileManagerAction`) opens the File Manager
   on the item's folder with the item selected and runs Quick Look there,
   the same route Get Info takes. The desktop's menus choose whether to offer
   it.
5. **Recents is a virtual place over the existing store.** `recents:` is a
   location like `applications:` (ADR-0262): `RecentsDirectoryLister`
   decorates the window's lister and answers it by reading
   `recently-used.xbel` read-only with a bounded reader (16 MiB file, the
   300 most recently used rows, four candidates examined per row); each
   bookmark's time is the latest of its added, modified and visited stamps,
   as `KRecentDocument` reads them. Only local `file:` paths that still exist
   are listed, each as the entry its own folder would list (same identity),
   so opening, previews, Quick Look, Get Info and item operations work
   unchanged. A missing store is an empty place; a damaged, foreign or
   oversized one is a typed listing error, never a partial list. Recents has
   no parent and one breadcrumb, sits in the sidebar (never a drop target)
   and under Go ▸ Recents (Ctrl+Shift+F, Finder's Shift+Cmd+F); folder
   actions that would create in, paste into, bookmark, describe or open a
   terminal in "the folder" are disabled there.

## Consequences

- One shortcut and one route per command: the File menu, Ctrl+Y, Space,
  the context menu and the desktop's reveal all end in
  `QuickLook.handle("file.quick-look")`.
- The File Manager never writes the recently-used store. Recents shows what
  applications record there; the File Manager's own launches appear when the
  launched application records them, as KDE and GTK applications do.
- Recents takes the window's sort order like any folder, and its per-folder
  view (ADR-0270) remembers a sort chosen there; it does not open sorted by
  last use until a sort key for the store's time exists.
- ADR-0260 said the desktop menu offers no Recents because the File Manager
  had none; `go.recents` now exists in the shared catalog for the desktop
  menu to adopt.
- Quick Look's arrows step through the folder's order (previous, next),
  not by grid rows.

## Alternatives considered

- **A Space menu shortcut.** Rejected: it would take Space from the location
  bar, filter and rename fields.
- **Reading the store with KIO's `KRecentDocument::recentUrls()`.** Rejected:
  it reads the whole file unbounded, logs a warning when the file is absent
  (fatal under the QML tests' fatal-warning rule) and returns oldest first;
  the File Manager's bounded-read rule favours a small reader of the same
  format.
- **KDE's activity statistics database.** Rejected: KDE-only, SQLite, and a
  second view of the same history that GTK applications never write.
- **A File Manager-owned history.** Rejected: a second store that misses
  everything opened elsewhere.

## Related

- [File Manager](../apps/file-manager.md#quick-look-type-to-select-and-recents)
- [ADR-0262: Applications is browsed in the File Manager's ordinary views](0262-applications-is-browsed-in-the-file-managers-ordinary-views.md)
- [ADR-0270: File Manager views move to QindaTK](0270-file-manager-views-move-to-qindatk.md)
- [ADR-0273: Integrate the File Manager with the desktop like Finder](0273-integrate-the-file-manager-with-the-desktop-like-finder.md)
