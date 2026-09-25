# ADR-0270: File Manager views move to QindaTK

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** First-party applications (File Manager)
- **Supersedes:** [ADR-0116](0116-build-bundled-applications-on-stock-qt6.md)
  for the File Manager's folder views only (the window's chrome, dialogs and
  menus stay on stock Qt Quick Controls), and the `preferences-v1` schema of
  [ADR-0198](0198-file-manager-preferences-are-app-local.md), whose store,
  refusal rules and app-local ownership stay in force.
- **Superseded by:** None

## Context

The File Manager had two views on stock Qt Quick Controls (ADR-0116): an
icon grid and a Details list with four fixed columns. Jarrod asked for more
views and Details customisation, on QindaTK. `preferences-v1` (ADR-0198)
remembered one global view, sort and zoom; its reader refuses any document
with other keys, so any new preference needs a new schema version.

QindaTK r5 (installed as `dev-libs/qindatk`) has the controls the views need
-- `Tk.Thumbnail`, `Tk.Filmstrip`, `Tk.Segmented`, `Tk.DataTable` -- but
`Tk.DataTable` tracks one current row and has no group headings, and none of
them knows the window's multi-selection, rubber band, drag and drop or
context menu. Every place (folders, search results, network locations, Trash,
Applications per ADR-0262) is served by the one `NavigationController`
listing, and must work in every view. Folders can hold 20,000 entries.

## Decision

1. **Four views on QindaTK**, all projecting the same `NavigationController`
   listing and the same `EntrySelection`: **Icons** (`Tk.Thumbnail` tiles
   with previews from the bounded ADR-0111 pipeline), **Details**
   (`Tk.DataTable`), **Columns** (macOS column view: read-only folder levels
   above, the browsed folder, a preview column) and **Gallery** (a large
   preview, key facts and a `Tk.Filmstrip` strip). `viewMode` keeps its
   historical keys `grid` and `list` and gains `columns` and `gallery`. A
   `Tk.Segmented` switcher and View-menu entries (Ctrl+1 Details, Ctrl+2
   Icons as before, Ctrl+3 Columns, Ctrl+4 Gallery) in the shared public
   catalog (ADR-0260) choose them.
2. **Selection, keyboard, rubber band, drag and drop and the context menu
   stay the window's own**, shared by every view (`EntrySelection.click()`,
   `ViewportNavigation`, `SelectionBand`, `EntryDrag`, `FileContextMenu`).
   The Details table's model is entry indexes plus heading rows; its own
   current row is unused, and its Name cell carries each row's behaviour.
3. **Group By is part of the listing order** (`ListingOrder.group`: none,
   kind, date modified, size), so groups are contiguous in the one index
   space every view and every file operation share; Details draws headings
   where the label changes. New sortable columns: Date Created, Date Accessed,
   Extension, Path, Permissions. Columns whose values are read lazily (Owner,
   Group, Items, Dimensions) are shown but never sort keys.
4. **Metadata for visible rows only.** The listing carries what its stat
   already gave (created, accessed, owner and group ids). Item counts, image
   dimensions and user/group names come from `EntryFacts`, a bounded,
   cached, one-thread reader asked only by visible cells. `EntrySelection`
   copies the listing once per change so no delegate re-marshals it.
5. **Per-folder views in `preferences-v2`.** A folder the user changes (view,
   order, grouping, zoom or Details columns) is remembered, at most 64
   folders, most recent first; a view equal to the defaults is forgotten.
   As in Finder, a folder with its own view opens in it and any other folder
   keeps the window's view; the defaults are what a window starts with, and
   they reach the open window when they change unless its folder has its own
   view. Walking the Columns view never leaves Columns. **Use as Defaults**
   makes the folder's view the defaults. Window-wide Details preferences:
   relative dates, compact rows, show extensions. `preferences-v2.json` is
   read first; only when it is absent is `preferences-v1.json` read and
   migrated, and it is never written again. A refused v2 never falls back.
6. **Theming stays ADR-0116's platform palette.** The File Manager publishes
   no QST tokens, so `ToolkitTheme` feeds the window palette into
   `Tk.Theme`'s base roles instead of `QindaTK.QindaQt`'s bridge. Nothing
   links QindaTK; it is a QML import, as for the System Monitor.

## Consequences

- `EntryGrid.qml`, `EntryList.qml`, `EntryListDelegate.qml` and
  `PresentationDefaults.qml` are removed; `FolderViewStack` owns the views
  and `FolderViewSettings` the per-folder state. `EntryReveal` and
  `ApplicationsPlaceActions` reveal entries through the active view's
  `revealIndex()`.
- The Details sort-header buttons (`sortHeader_*`) and the view toggle button
  are gone; the UI probes ask the `detailsTable` to sort and look for the
  `viewSwitcher` and the four view objects instead.
- The Gallery preview uses a second preview provider (`gallery-previews`)
  with a 1024-pixel bound and the same input limits, identity checks and
  generation fencing as thumbnails.
- `preferences-v2` is larger (a 128 KiB bound) and older builds keep reading
  their untouched v1 file.
- The package depends on `dev-libs/qindatk` with Thumbnail, Filmstrip,
  Segmented and DataTable (r5).

## Revisit when

- QindaTK's DataTable gains multi-selection, group headings or its own row
  drag: the Name cell's row overlay can then shrink to the table's own.
- The File Manager moves its chrome to QindaTK, or gains QST tokens, so the
  QindaTK.QindaQt bridge can replace `ToolkitTheme`.
- Users need more than 64 remembered folders, or remembered views on
  removable media that change path.
