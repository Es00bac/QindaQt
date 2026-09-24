# ADR-0262: Applications is browsed in the File Manager's ordinary views

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** First-party applications (File Manager)
- **Supersedes:** The drill-down presentation of
  [ADR-0164](0164-shared-application-catalog-and-file-manager-applications-browser.md)
  (decision 3's folders/entries/breadcrumb browser). ADR-0164's shared catalog,
  launch planning and launch policy stay in force.
- **Superseded by:** None

## Context

ADR-0164 gave the File Manager an Applications browser: a separate screen
(`ApplicationsView.qml`) with a fixed strip of category "folders" above a
plain list of entries. A single click launched. It had no icon grid, no
selection, no keyboard navigation, no zoom, no filter, no sorting and no
context menu. It did not behave like the rest of the File Manager, or like
the macOS Applications folder people expect. ADR-0172 made Applications a
sidebar place and added the docked "takes its place" launch. ADR-0165 reuses
the browser as the workspace picker (`--choose-application`).

The File Manager already has two complete views, `EntryGrid` (icons) and
`EntryList` (details). Both are driven by `NavigationController` through
its injected `DirectoryLister` and `FileLauncher` seams, and both already
have selection, rubber-band, type-to-select, the filter bar, zoom, sorting
and context menus. Workstream W11 will rebuild these views on QindaTK
(superseding ADR-0116 for the view layer), so any Applications behaviour
placed inside the views would have to be rewritten there.

## Decision

1. **Applications is a virtual location, `applications:`, that the window's
   `NavigationController` browses like a folder.** The address has no `/`,
   so `QDir::cleanPath()` keeps it unchanged and `NetworkLocation` classifies
   it as local. `NavigationHistory` gives it no parent and a single
   "Applications" breadcrumb. `ApplicationsLocation` (header-only) is the
   one place that spells the address.
2. **The place reaches the navigation through its two existing seams.**
   `ApplicationsDirectoryLister` wraps the real lister and answers the
   location with `ApplicationsController::listing()`. Every listing, and F5,
   rescans the catalog synchronously within the catalog's limits.
   `ApplicationsFileLauncher` wraps the real launcher and sends a row path
   (`applications:<desktop-id>`) to `ApplicationsController::open()`. Any
   other path goes to ADR-0029's document launcher unchanged.
   `NavigationController` learns nothing about applications beyond one
   read-only `applicationsPlace` flag. It is not otherwise modified.
3. **Each application is one `DirectoryEntry` row.** The row carries its
   display name, the row path, the desktop-entry id (`applicationId`), the
   theme icon, its primary category group as the Kind text, and a note
   explaining why it cannot start from a standalone window, if it can't.
   Size and modification time are unknown and show as a dash. Rows come
   only from `application_catalog`, and `NoDisplay`/`Hidden` entries never
   appear. `ApplicationsListing` holds these pure projections plus the
   Get Info fields.
4. **Behaviour lives in the model and controller layer, feeding the standard
   views.** Sorting (A to Z by default), filtering, type-to-select, zoom and
   selection are the folder ones. **View ▸ Group by Category** (Ctrl+G)
   sorts by Kind, where an application's Kind is its category, and the
   details view adds a heading row for each category. The drill-down strip
   is removed. `ApplicationsPlaceOrder` keeps the place's sort separate from
   the window's folder sort, so grouping never reorders ordinary folders and
   a folder's date sort never reorders Applications. The views change only
   additively: a Category header label, the section headings, a dimmed
   name for rows with a note (the note is also the row's tooltip and
   accessible description, so the state is never shown by colour alone),
   and an accessible "application" suffix.
   **W11 must carry these over when it moves the views to QindaTK. It must
   keep consuming the same `NavigationController` rows and not
   re-implement Applications in the view layer.**
5. **Opening follows every other folder.** A single click selects. A
   double-click, Enter, **File ▸ Open** (Ctrl+O) or the context menu's
   **Open** calls `NavigationController::activate()`. The launch policy is
   still ADR-0164/ADR-0172's: ask the compositor first, so a docked window
   is replaced in place. If the compositor declines, fall back to a detached
   plain-process launch. A terminal-required or D-Bus-activatable entry
   reports its note in the window's launch banner. Chooser mode (ADR-0165)
   opens directly into Applications and passes every activation to the
   compositor only. `ApplicationLaunchSeams` lets tests record both effects
   without calling a bus or starting a process.
6. **The context menu for an application row offers:** Open, Get Info
   (`file.properties`, shown in an `ApplicationInfoDialog` in the Properties
   dialog pattern), and Show Desktop Entry File
   (`application.show-entry-file`, Ctrl+Shift+E). Show Desktop Entry File
   navigates to the entry's folder and selects it there. The background
   menu offers Group by Category. File actions (cut, copy, paste, rename,
   Copy To, Move To, trash, New Folder, bookmark) are disabled and hidden,
   because application rows are not files. Application rows never enter a
   drag, and nothing can be dropped into the place.
7. **Left out, because no public boundary exists yet:**
   - **Keep in Dock.** The dock's pins are the Settings1 key
     `panels.launcherPinned`, whose only writer and validator is the shell
     launcher controller (ADR-0076). W13 replaces that key with
     `panels.dockItems`.
   - **Add to Desktop.** The desktop surface lists `~/Desktop` as files and
     opens them through ADR-0029's handler dispatch. It has no
     trusted-launcher contract for copied desktop entries.
   - **Open in New Workspace.** The compositor's only application route is
     `ChooseApplicationForActivePicker` (picker or docked member).
   - **Sort by recently used.** The only launch history is the shell
     launcher's `panels.launcherRecent`, and no second history store is
     created.
   - **Dragging an app to the dock or desktop.** Neither accepts an
     application drop.
   - **Dropping files on an app.** This is W10's Open With path.

## Consequences

- Applications behaves like every other folder in both views, and gains
  every future view feature for free, provided that feature consumes
  `NavigationController` rows.
- Only one catalog authority (ADR-0164) and one launch authority
  (`ApplicationsController`) remain. The decorators never read or execute a
  desktop entry, and a row path that reaches the document launcher or
  mutation pipeline fails closed as a non-existent file.
- The icon view has no section headings. There, Group by Category is the
  category sort only. Headings in the icon view are left to W11's QindaTK
  views.
- Each visit to the place rescans the catalog on the GUI thread. The
  startup scan already did this, and it stays bounded by the catalog's
  root, entry and size limits.
- `ApplicationsPlaceOrder` swaps the sort through the controller's public
  `setSortColumn()`. If per-folder view settings arrive (W11,
  `preferences-v2`), they should replace it.

## Revisit when

- W13 publishes a dock-item boundary (Keep in Dock, drag to dock).
- The desktop gains a trusted-launcher contract (Add to Desktop).
- The compositor gains a "launch into a new workspace" route.
- W11 per-folder view settings land (retire `ApplicationsPlaceOrder`).
