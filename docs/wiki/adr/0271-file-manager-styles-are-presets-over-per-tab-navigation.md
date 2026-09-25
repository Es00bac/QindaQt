# ADR-0271: File Manager styles are presets over per-tab navigation

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** First-party applications (File Manager)
- **Supersedes:** None. Extends [ADR-0270](0270-file-manager-views-move-to-qindatk.md)'s
  `preferences-v2` before it shipped, and fills the `workflow.fileManager`
  placeholder of [ADR-0268](0268-familiar-desktop-experiences-are-layout-and-theme-pairs.md).
- **Superseded by:** None

## Context

Jarrod asked for the File Manager to work like Finder, Windows Explorer or a
Commander-style file manager without becoming three applications, and for
tabs. The desktop layouts already name the arrangement they expect
(`workflow.fileManager`, ADR-0268): Mac-like layouts `finder`, the
Windows-like ones -- Windows 3.1's tree beside the list included --
`explorer`.

A window had exactly one `NavigationController`, and the AppShell action
states (enabled Back, Paste, Copy To …), search results and preview fencing
were bound to it once in `main.cpp`. Tabs and Commander's two panes each need
a folder, history, listing and selection of their own at the same time.

## Decision

1. **One setting, three styles, as presets.** Preferences ▸ General ▸ File
   manager style: **Match the desktop layout** (the default), **Finder**,
   **Explorer**, **Commander**. It is stored in `preferences-v2` as
   `fileManagerStyle` ("" = match the layout). A style turns on chrome the
   window already has or two small additions, and picking one writes its
   starting view into the existing "Open folders as" preference in the same
   write (Finder Icons, Explorer and Commander Details). Folders with a view
   of their own keep it.
   - **Finder**: today's window.
   - **Explorer**: a folder tree under the sidebar's places (new,
     `FolderTree`), the breadcrumb as a framed address bar that turns into
     the location field when its empty part is clicked, and a compact
     command bar (New, Cut, Copy, Paste, Rename, Delete, Sort, View) whose
     buttons are catalog actions.
   - **Commander**: two panes side by side (new, `FolderPanes`), each with
     its own folder, view and tabs; Tab switches panes; Copy To and Move To
     start at the other pane's folder; a bottom bar and keys F3 View (Quick
     Look, ADR-0272),
     F4 Edit (Open With), F5 Copy, F6 Move, F7 New Folder, F8 Delete
     (Move to Trash).
2. **The layout's hint is the style until the user picks one.**
   `LayoutStyleHint` reads Settings1 `panels.layoutProfile` live and the
   profile catalog's `workflow.fileManager` once. While following it, a
   layout's preset is written once per layout change (`layoutStyle` records
   the last applied), so "Open folders as" stays the user's afterwards.
   An unknown hint, or none, is Finder.
3. **Every tab browses with its own `NavigationController`.**
   `FolderNavigations` makes the controllers after the window's first from
   the composition root's recipe and owns them until a tab closes. The
   window's commands follow the tab the user works in: `setActive()` rebinds
   every window-wide seam (the five action binders, search results, preview
   generations) under a context object it destroys to unbind, so the binders
   gained an optional `context` parameter. The first tab keeps the injected
   controller and is only hidden when closed, so the window always has one.
   Swapping one controller's folder and history between tabs was rejected:
   every switch would re-read the folder and lose the selection and scroll,
   and Commander needs two live listings at once.
4. **Tabs belong to panes.** Ctrl+T opens a tab at the current folder,
   Ctrl+W closes the current one (the last tab of a single pane closes the
   window), Ctrl+Tab and Ctrl+Shift+Tab move between them, a file dropped on
   a tab moves (Ctrl copies) into its folder. In Commander each pane has its
   own tabs.
5. `fileManagerStyle` and `layoutStyle` join `preferences-v2` without a new
   version because v2 has not shipped (it is ADR-0270's, in the same round).

## Consequences

- `FolderViewSettings` and `EntrySelection` are per tab (`FolderTab`), no
  longer the window's; Main.qml reaches them only through `FolderPanes`
  (`activeNavigation`, `activeSelection`, `activeViews`).
- F5 is Refresh in the shared catalog; in a Commander pane the pane claims
  F5-F8 at ShortcutOverride, so they win there. Ctrl+R still refreshes.
- Only the active controller's listing generation fences previews, so a
  background Commander pane that re-reads its folder shows icons until the
  user works in it.
- The tree re-reads its open folders (one bounded synchronous read each)
  whenever it is rebuilt; it keeps no stale copy.
- Tabs have keys and a tab bar but no menu entries yet: the menu catalog is
  shared with the desktop (ADR-0260), which has no tabs.
- The File Manager links the static profile loader.

## Revisit when

- The File Manager gains file watching (hidden tabs and the tree would then
  follow changes made elsewhere).
- Tabs need menu entries or a per-window restore of their folders.
