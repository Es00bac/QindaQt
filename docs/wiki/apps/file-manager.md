# QindaQt File Manager

`qindaqt-file-manager` is QindaQt's first-party local-directory browser. S0
landed bounded listing, navigation, and regular-file launch. S1 adds local new
folder, rename, copy, same-filesystem move, home Trash, restore, and
empty-Trash operations with cooperative cancellation, progress, typed failure,
and one-level recovery. S2 adds the core browsing surface: an editable
location bar, multi-select with serialized batch operations (including the
rubber-band marquee gesture and alternating Details-row backgrounds), configurable
sorting with size/kind/modified columns, a hidden-file toggle, a list/grid
view switch, and a places/bookmarks sidebar persisted in an app-local state
file. The visual browsing revision adds catalog icons and bounded local raster previews. S3 adds the daily-use
file clipboard (cut/copy/paste), drag-and-drop, a bounded recursive search,
and a properties dialog. S5 adds read-only `smb://`/`sftp://` network-location
browsing behind an injected asynchronous backend seam (ADR-0137), opens
remote regular files through the desktop's default handler via the injected
`RemoteFileOpener` seam on `KIO::OpenUrlJob` (ADR-0152), and renames a
listed child through the injected `RemoteRenamer` seam on `KIO::rename()`
(ADR-0153), creates one validated child directory through the injected
`RemoteFolderCreator` seam on `KIO::mkdir()` (ADR-0154), copies one listed
child to a validated remote destination folder through the injected
`RemoteCopier` seam on `KIO::copy()` (ADR-0155), moves one listed child
the same way through the injected `RemoteMover` seam on `KIO::move()`
(ADR-0156), and writes a remote file's edits back in place by resolving
the canonical URL through the session KIOFuse service and opening the
returned local write-back path through the same `KIO::OpenUrlJob` boundary
(ADR-0157). S6 makes those locations first class: saved network locations
with a Network hub page and a Connect-to-server dialog (ADR-0194), and a
transfer queue that copies and moves between the local machine and a network
location in either direction (ADR-0195), with sign-in left to the platform
(ADR-0196). S7 completes that area: nearby servers over the platform's Avahi
(ADR-0200), a preferences window whose settings survive a restart (ADR-0198),
and a per-location "mount at login" knob that writes one systemd user unit
(ADR-0199). Per-volume Trash and portal locations remain later slices (see the
roadmap below).

The durable local-launch choice is recorded in
[ADR-0029](../adr/0029-file-manager-bounded-local-launch.md); the S1 mutation
and Trash authority is recorded in
[ADR-0064](../adr/0064-confine-file-mutation-to-identity-checked-local-authority.md);
the S2 bookmark persistence contract is recorded in
[ADR-0090](../adr/0090-keep-file-manager-bookmarks-app-local.md).
Bounded local previews and public icon composition follow
[ADR-0111](../adr/0111-bound-file-previews-and-consume-public-icons.md).
The stock Qt 6 presentation surface follows
[ADR-0116](../adr/0116-build-bundled-applications-on-stock-qt6.md). The S5
network-location browsing seam and KIO adapter are recorded in
[ADR-0137](../adr/0137-file-manager-network-location-browsing.md), and the
remote Copy To and Move To seams are recorded in
[ADR-0155](../adr/0155-file-manager-remote-copy-to.md) and
[ADR-0156](../adr/0156-file-manager-remote-move-to.md), and remote
write-in-place through the platform KIOFuse service is recorded in
[ADR-0157](../adr/0157-file-manager-remote-write-in-place.md).

File Manager's presentation is stock Qt 6 QML (`QtQuick`, `QtQuick.Controls`,
`QtQuick.Layouts`) styled by the platform theme palette; it no longer imports
`QindaQt.Tokens` or `QindaQt.Controls` (ADR-0116). It still composes the
public `QindaQt.AppShell 1.0` window/action/lifecycle boundary. File Manager
retains all navigation and filesystem policy; AppShell owns only the standard
menu, shortcut dispatch, focus reporting, and close-decision protocol (see
[Module boundaries](../architecture/module-boundaries.md)).

## First-party global-menu export

File Manager opts its deterministic AppShell action catalog into
`QindaQt::AppShell::MenuExport` after the real `ApplicationShell` window is
constructed. With no usable session bus, registrar owner, or platform window
identity, export remains disabled/waiting and the application continues
normally. XWayland registers the real window id; native Wayland announces the
unique bus name and dbusmenu path through Qt's KDE appmenu platform hook, with
no invented numeric id. The shell independently authenticates those facts
against the focused surface and provider PID.

The in-window `MenuBar` remains visible and authoritative. Registrar presence
or successful publication is not evidence that a shell actually hosts the
menu. Either local or shell activation enters the same
`ApplicationCoordinator::activateAction()` enabled-action gate, and the shell
path emits the File Manager action request exactly once.

## Browsing experience

One window browses one local folder tree at a time, starting at the user's
home folder unless a valid local folder is given on the command line. There
are deliberately no in-window tabs or split panes: grouping windows is the
[window container](../architecture/window-containers.md)'s job, and a file
manager window composes with it like any other application window.

A single compact toolbar combines icon buttons for Back, Up, New Folder and
view mode with clickable breadcrumbs. The current folder and at most two
ancestors are shown (one ancestor at compact widths); a leading parent-folders
menu retains the full hierarchy. `Ctrl+L` always reveals the complete path.
Back and Forward remain visible at compact widths, and mouse Back/Forward
buttons use the same history actions. Toolbar and sidebar use flat semantic
surfaces with thin dividers; no gradient is painted behind navigation controls. Folder Options
holds direct location entry, hidden files, refresh, sorting and Trash recovery.
`Ctrl+L` replaces the breadcrumbs with an editable location field; Enter uses the
same normalized navigation boundary, and Escape restores the breadcrumbs.
Successful asynchronous network navigation also restores the breadcrumbs once
the listing arrives; failed navigation keeps the location field available for
correction. Closing location entry returns keyboard focus to the folder view.
Folder-specific menu actions and shortcuts are disabled while the Network hub
is visible, including actions on a retained file selection; inside the
Applications place only the file actions are disabled (see
[Applications browser](#applications-browser)).
The window supplies the nonpersistent `folderViewActive` presentation flag to
the existing browsing, mutation, and clipboard action bindings. Home and
location entry return to the folder view even when its path is unchanged.
Folder navigation leaves the Network hub through the controller's
`navigationChanged` notification. New Folder is available in the
toolbar for remote locations that support creation, and disables while their
creation operation is pending.
Tooltips and accessible labels explain every icon action.

The places sidebar offers fixed places (Home, File System, Trash) and the
user's bookmarks. `Ctrl+D` bookmarks the current folder; each bookmark row
has an icon button to remove it. Places retain both recognizable icons and labels;
Places, saved Network locations and Bookmarks share one scrollable viewport,
so a long network inventory cannot push bookmarks outside a short window.
Keyboard focus reveals the focused row, including each remove button.
Removing the last bookmark collapses the empty bookmark list so its old height
does not leave blank scrollable space in the sidebar.
Preferences pages likewise scroll within their window; the common Close and
Restore Defaults footer stays reachable when help or errors make a page tall.
the sidebar narrows from 196 to 148 pixels below 680 pixels window width. Bookmarks persist across restarts through
`BookmarksStore` (ADR-0090); a bookmark whose folder vanished simply lands on
the ordinary "missing" state card.

The main pane defaults to a spacious icon grid. Both views support rubber-band
(marquee) selection: dragging on empty viewport space draws a band and selects
every crossed entry on release, with the usual modifier policy (a plain band
replaces the selection, Shift unions, Control toggles); a band-free click on
empty space clears it. Selection also composes from click, Ctrl+click, and
Shift+click range gestures, `Ctrl+A`, and keyboard navigation. Details mode
supports additive Ctrl+Shift ranges just like the icon grid, with the same
behavior from the keyboard. Selection identity includes each entry's path so
same-name hard links in different recursive-search folders stay distinct.
Details mode
alternates row backgrounds (odd rows carry the palette's `alternateBase`, even
rows stay transparent, hover is a translucent highlight tint on either parity,
and the selection highlight always wins) so adjacent rows are easy to tell
apart. Details mode exposes a clickable
sort header (Name, Size, Kind, Modified); metadata columns progressively hide
below the available width, preserving the filename and size. Clicking the active column reverses its
direction; directories sort first by default. Hidden entries (dot names) are
filtered out of the published listing by default; `Ctrl+H` or the toolbar
toggle shows them at their sorted positions, and the status notice reports
the filtered count ("3 hidden"). List mode shows preformatted size, kind, and
modified columns; grid mode shows the same entries as catalog/MIME icons with local image previews. Both modes
share one selection contract. Both views and the complete places sidebar have draggable
vertical scrollbars that remain visible while content overflows. Their reserved
gutters prevent icons or filenames from sitting beneath the thumb. Original folder artwork adds a decorative empty
state at roomy sizes, while compact windows retain the accessible state card.

| Action identity | Shortcut | Meaning |
| --- | --- | --- |
| `go.back` / `navigateBackButton` | `Alt+Left` | Return to the previous folder in history |
| `go.forward` / `navigateForwardButton` | `Alt+Right` | Return to the folder undone by Back |
| `go.up` / `navigateUpButton` | `Alt+Up`, `Backspace` (when the list has focus) | Go to the parent folder |
| `view.refresh` / `refreshButton` | `F5`, `Ctrl+R` | Re-read the current folder |
| `view.focus-location` | `Ctrl+L` | Swap the breadcrumb for the editable location field |
| `view.show-hidden` | `Ctrl+H` | Show or hide dot-name entries (checkable) |
| `view.details-mode` / `view.grid-mode` | `Ctrl+1` / `Ctrl+2` | Select Details / Icon view directly |
| `view.zoom-in` / `view.zoom-out` | `Ctrl++` (`Ctrl+=` also accepted) / `Ctrl+-`, or `Ctrl+wheel` | Increase / decrease icon size in the current view |
| `view.zoom-reset` | `Ctrl+0` | Restore the default icon size |
| `view.filter` | `Ctrl+F` | Focus the current-folder filename filter |
| `edit.cut` | `Ctrl+X` | Cut the selected entries to the file clipboard |
| `edit.copy` | `Ctrl+C` | Copy the selected entries to the file clipboard |
| `edit.paste` | `Ctrl+V` | Paste clipboard files into the current or focused folder |
| `file.properties` | `Alt+Return` | Open the properties dialog for the selection |
| `edit.select-all` | `Ctrl+A` | Select every visible entry |
| `go.home` | `Alt+Home` | Open the home folder |
| `bookmark.add` | `Ctrl+D` | Bookmark the current folder |
| `entryListView` / `entryGridView` | `Return`/`Enter` | Open the selected entry |

The status bar reports the visible item or selection count and provides zoom
buttons plus a reset percentage. Zoom uses seven bounded, session-local icon
sizes (16, 24, 32, 48, 64, 96, 128 logical pixels; 64 is the default). Details rows scale
their icons and height proportionally. Zoom leaves the view mode unchanged;
`Ctrl+1`/`Ctrl+2` select a mode explicitly. These catalog commands are not
checkable toggles: repeating the current view does nothing. Folder Options
uses exclusive view choices to indicate the current mode. Plain wheel input scrolls, while
Ctrl+wheel accumulates fine wheel/trackpad deltas into zoom steps and does not
also scroll. Resizing or zooming keeps the current item visible without
changing its selection identity. Keyboard focus starts in the file view when
the folder is ready; Page Up/Down move by the visible page, and typing a name
selects a matching item (repeated initial letters cycle matches).

`Ctrl+F` opens **Filter this folder by name**. It performs a case-insensitive
literal substring match over the current loaded listing, including visible
hidden names only when Show Hidden Files is enabled. Input is bounded to 256 UTF-16 units without splitting a surrogate
pair. Refresh, sorting, and view changes retain the filter; navigating to a
different folder clears it. Enter returns focus to browsing, and Escape clears
and closes the filter. File identity and preview generations are retained;
filtered-out entries cannot stay invisibly selected for a file operation.

The filter bar's **Subfolders** toggle widens the same literal substring match
into a bounded recursive search rooted at the current folder
(`SearchController`): one worker thread, at most 2000 results, 24 levels deep,
and 100000 visited entries, never descending through symbolic links. Results
replace the listing as a guest result set (the status text reports the match
count or the reached limit); the window's current folder does not change, and
any navigation, refresh, or filter close discards it. A committed file
operation restarts the in-flight search so the result set reflects the new
tree state. Case-insensitivity, hidden-file opt-in, and input bounds match the
plain filter. Clearing the query also discards the recursive result set.
Closing the filter or navigating elsewhere cancels its pending debounce and
worker so a delayed search cannot replace the new view.

Selection is shared between list and grid views. Ctrl-click toggles individual
files; Shift-click and Shift+arrows select a range; Ctrl+A selects all visible
entries. Plain arrows select the destination, while Ctrl+arrows move only the
focus outline. Grid Up/Down move by a row. Right-clicking an already selected
file keeps the batch selected for the context menu.

Sorting and refreshing keep the same selected files, identified by name,
path, device and inode. Filtering drops entries that are no longer visible; revealing
them again does not silently reselect them. Opening another folder clears the
selection. The focus outline may start at the first item, but that alone does
not select it for a file operation. Switching list/grid preserves both focus
and selection. Batch dispatch retains the selected entries' original identity
snapshots so a changed file is checked by the mutation backend rather than
silently substituting newly observed data.

Opening a directory entry navigates into it. Opening a file entry requests a
bounded local launch (see below); the list selection and current folder never
change just because a launch failed. A folder that cannot be listed (missing,
not a folder, permission denied, or an unclassified read failure) or that
lists cleanly but has no children presents one accessible state card built
from stock Qt Quick Controls instead of an empty or frozen-looking list. A
ready folder whose entries are all hidden stays in the Ready state with an
empty list and the "N hidden" notice rather than claiming the folder is
empty.

## Bounded visual previews

The private `PreviewDecoder` seam and engine-owned `PreviewProvider` keep image
I/O out of QML. At most two jobs decode PNG, JPEG, BMP or WebP locally. Inputs
are limited to 32 MiB and 40 megapixels; previews are at most 192 × 192 pixels,
with a 64 MiB memory LRU and no disk cache. Icons stay visible behind previews
so unsupported, unreadable, oversized and corrupt images remain recognizable.

A URL includes the exact listing identity/revision and generation. Navigation
or refresh cancels obsolete requests; the decoder checks a pinned regular-file
descriptor before/after decoding and the current path afterward. Cache hits
also recheck identity, and generation checks fence publication. Cancellation is
cooperative around the codec call; destruction cancels and joins workers.
Symlinks retain icons and are not previewed. Sorting and selection keep their
existing independent identities and do not gain preview policy.

## S1 local mutation and recovery

All mutation actions enter through the AppShell action catalog. The list
context menu and keyboard/menu surface dispatch the same stable action
identity; destructive actions require confirmation before reaching the
controller.

| Action identity | Shortcut | Result |
| --- | --- | --- |
| `file.new-folder` | `Ctrl+Shift+N` | Create a named folder in the current directory |
| `file.rename` | `F2` | Rename the selected item without changing its parent |
| `file.copy` | `Ctrl+Shift+C` | Copy the selected file or tree to an absolute local path |
| `file.move` | `Ctrl+Shift+M` | Move the selected item to an absolute path on the same filesystem |
| `file.trash` | `Delete` | Confirm and move the selected item(s) to recoverable home Trash |
| `file.restore-last` | `Ctrl+Shift+R` | Restore the most recently trashed item to its recorded path |
| `file.empty-trash` | `Ctrl+Shift+Delete` | Confirm and permanently empty home Trash |
| `edit.undo` | platform Undo | Undo the last recoverable create, rename, or move |
| `operation.cancel` | `Ctrl+Escape` | Request cancellation of the running operation |

### Background and selection context menus

`FileContextMenu.qml` is the one shared right-click/keyboard context menu
`EntryGrid` and `EntryList` both instantiate, so Icon and Details behave
identically instead of each declaring its own item list. Right-clicking (or
invoking the context-menu key on) empty folder space shows only background
actions — New Folder, Paste (only when the clipboard actually holds
something), Refresh, the Icon/Details toggle, and Show Hidden. Right-clicking
(or invoking the context-menu key while) a selection is targeted shows only
`edit.cut`/`edit.copy`, `file.copy`/`file.move`/`file.trash`/`file.properties`,
and `file.rename` — Rename only when exactly one entry is targeted, since
`MutationDialogs.dispatch("file.rename")` only ever acts on the first selected
entry. Every item dispatches through the same
`ApplicationCoordinator::activateAction()`/`MutationDialogs` path the toolbar
and menu bar already use; the menu adds no new mutation policy, only
context-scoped visibility.

A mouse right-click on an unselected entry replaces the selection with that
entry first (mirroring a plain left-click), so the menu always targets what
was actually clicked. The context-menu key (`Menu`, or `Shift+F10`) follows
the same rule for the keyboard: with a valid focused entry it selects that
entry first if nothing is already selected, then targets the resulting
selection; with no focused entry (an empty folder, including a Ready folder
whose only entries are hidden) it targets the background instead. A
background right-click never changes the existing selection.

The focused row `qindaqt.file-manager-browsing-ui` drives all of this through
production `Main.qml` at compact and desktop sizes in both view modes: real
mouse clicks distinguishing background from single- and multi-entry
selection, the pristine-focus and true-background keyboard cases, and a real
New Folder/Paste round trip through the existing dialogs and controllers.

With multiple entries selected, copy and move ask for a destination **folder**
and trash confirms the count, then run as one serialized batch:
`MutationController::copyItemsTo`/`moveItemsTo`/`trashItems` validate every
item's path plus listing-time identity up front, then execute the existing
single-item, identity-checked backend contract per selection entry in order
inside the one busy slot. The first typed failure stops the batch and reports
"Completed N of M items" with the failing name; cancellation between items
skips the remainder. Batch operations intentionally carry no undo request and
no Trash restore token: a completed batch clears any pending one-level undo,
and `restore-last` keeps referring to the most recent single-item trash. This
mirrors the deliberate S1 choice that copy has no undo.

`MutationController` is GUI-thread confined and owns one injected
`MutationBackend`, one worker thread, and at most one in-flight operation.
It publishes bounded progress and terminal state. Cancellation is cooperative;
controller destruction sets the token and joins the worker before releasing
the backend. Only successful create, rename, and move results replace the
one-level undo request. Trash instead retains one opaque identity-bearing
restore token; restore and empty Trash clear it.

Every request declares absolute local roots and carries the selected item's
listing-time identity: device, inode, size, nanosecond modification time, and
mode. These five fields cross the QML boundary as canonical decimal strings,
not JavaScript numbers, so 64-bit inode and nanosecond values remain exact when
an action returns the selected entry to C++. Create carries the parent identity.
Accepting Rename with the unchanged current name is a no-op. Before mutation,
the backend checks lexical containment, rejects every observed symbolic-link
component, and compares the current identity. Missing state returns `vanished`;
changed state returns `changed`; an existing destination returns
`already-exists`. Other
typed failures are `permission-denied`, `cross-device`, `disk-full`,
`symlink-escape`, `cancelled`, `unsupported`, `io-error`, and `busy`. User text
in diagnostics is sanitized and capped.

Recursive copy is limited to 20,000 entries. It pins source and destination
directories with descriptor-relative `openat`/`fstatat` calls, applies
`O_NOFOLLOW` at each kernel boundary, rechecks every visited identity, and
rejects symbolic links anywhere in the tree. It preserves permissions and
modification time where the platform permits and removes a partial destination
after cancellation, hostile content, change, or vanishing. Rename and move
commit with Linux `renameat2(RENAME_NOREPLACE)`, so a destination created by
another writer after preflight is preserved and returns `already-exists`
atomically. They preserve the filesystem's existing metadata; cross-device move
is refused rather than silently degrading to copy-and-delete.

### Home Trash contract

S1 implements the freedesktop.org
[Trash specification](https://specifications.freedesktop.org/trash/latest/)
only at `$XDG_DATA_HOME/Trash`. `info/` holds mode-0600 `.trashinfo` records;
`files/` holds the same-named payload. Metadata contains the percent-encoded
absolute source path and local `DeletionDate`, is created exclusively and
flushed before the payload rename, and is removed after successful restore.
Name collisions in either `info/` or `files/` receive a bounded numeric suffix;
an orphan payload left by another implementation therefore cannot wedge future
trashing of the same base name. The Trash root and both children are owner-only
and checked for symbolic-link substitution before use. A restore whose
destination parent vanished returns `vanished`, while `cross-device` is
reserved for two successfully resolved, unequal device identities.

The backend compares source and home-Trash device identities before rename.
If they differ, `cross-device` is returned and the source remains untouched.
Per-volume Trash remains deferred to the volumes slice. Empty Trash removes
entries below `files/` and `info/` without following links and retains those
two directories.

## File clipboard, drag-and-drop, and properties

`ClipboardController` owns the File Manager clipboard policy
(`edit.cut`/`edit.copy`/`edit.paste`, the view context menus, and drops). An
own cut/copy snapshot keeps the listing-time identity maps, so a paste is
dispatched through the same identity-checked batch contract as a context-menu
copy or move; a committed cut paste clears the snapshot so dangling sources
are never pasted twice, while a failed one keeps it for retry. Paste targets
the focused folder entry when there is one, else the current folder; pasting
an entry onto itself, its own parent, or its own descendant is refused before
dispatch. The controller also publishes the standard
`text/uri-list`/`x-special/gnome-copied-files`/KDE cut markers, and adopts
foreign clipboard content (another application's payload, re-stat'ed at
dispatch through `MutationController::copyForeignPathsTo`, at most 4096
paths). Foreign content is only ever copied — a foreign cut marker is
deliberately not honored, so another application's files are never moved out
from under it.

Drag-and-drop moves the selection within the window by default (Ctrl switches
to copy): onto folder delegates, onto the view background (the current
folder), and onto places/bookmark rows in the sidebar. The Trash place is
excluded — a direct drop would bypass the `.trashinfo` record the Trash
contract requires. Internal drags carry the JSON identity snapshot under a
private MIME type and reuse the same self/descendant guards as paste; foreign
URL drops enter as bounded copy-only `dropUrlsInto` dispatches.

`Alt+Return`, the File menu, and the context menus open a stock-Controls
properties dialog (`EntryPropertiesController`). A single entry shows name,
kind, MIME type, size, modification time, symbolic permissions, and full path;
a folder or multi selection shows the count plus a combined total size walked
on one bounded worker thread (at most 20000 visited entries, 32 levels, no
symlink descent, cancelled on re-inspect, close, or destruction).

## Bounded local file launch

Opening a file entry validates it synchronously before requesting a launch:
the target must exist, an existing symlink is resolved once to its canonical
target (mirroring the Text Editor's own open contract), the resolved target
must be a readable regular file, and only then is
`QDesktopServices::openUrl()` asked to hand it to the desktop's already
configured default handler. File Manager owns no MIME database, handler list,
or launched-process lifetime; a validation failure or a `false` return from
`openUrl()` becomes a typed `LaunchError` and a dismissible warning banner
rather than blocking navigation, crashing, or silently doing nothing. See
[ADR-0029](../adr/0029-file-manager-bounded-local-launch.md) for the full
rationale and boundary.

## Applications browser

**Applications** is a place in the sidebar, beside Home, File System, Trash and
Network, drawn with the `folder-applications` glyph
([ADR-0172](../adr/0172-applications-is-a-place-and-a-docked-window-can-replace-itself.md)).
Activating it, **Go ▸ Applications** (Ctrl+Shift+A) or typing `applications:`
in the location bar all open one virtual location that the window browses in
its ordinary Icon and Details views, exactly like a folder
([ADR-0262](../adr/0262-applications-is-browsed-in-the-file-managers-ordinary-views.md)).
The place is emphasized in the sidebar by that path. It is never a drop target
and has no parent: Up is disabled and the breadcrumb reads just "Applications".

Every launchable application is one item, sorted A to Z. Each item shows its
theme icon at the current zoom and its category as the Details view's
**Category** column. Size and date are unknown and show a dash. Entries come
only from the shared `application_catalog` module (launcher-L0 parsers, no
second parsing authority;
[ADR-0164](../adr/0164-shared-application-catalog-and-file-manager-applications-browser.md)).
`NoDisplay` and `Hidden` entries never appear. Selection, rubber-band,
keyboard navigation, type-to-select, zoom, the filter bar and sorting are
the folder ones. The filter bar's Subfolders toggle is disabled, because the
place has no subfolders. **View ▸ Group by Category** (Ctrl+G, also in the
background context menu) sorts by category, and the Details view adds a
heading for each category. The Icon view does not have headings yet. The
place keeps its own sort apart from the window's folder sort, so grouping
never reorders ordinary folders. Visiting the place or refreshing it (F5)
rescans the catalog.

A single click selects. A double-click, Enter, **File ▸ Open** (Ctrl+O) or the
context menu's **Open** opens the selected applications. Opening asks the
compositor first (ADR-0172). When this window is the active one and is docked
in a container, the chosen application **takes its place**: the compositor
launches it and swaps it in through the same atomic `ReplaceMemberWindow`
transaction a restored picker uses. That swap is gated on the bus daemon's
credential for the caller matching KWin's authenticated client PID for the
window, so a caller can only ever replace its own window. A rejection is the
ordinary undocked case. It falls back silently to a plain detached launch,
which starts an entry only when its planned argv is a plain process.
Terminal-required and D-Bus-activatable entries are dimmed. Their tooltip,
accessible description and Get Info say why. On that fallback they report
the reason in the window's "Couldn't open the application" banner. The
compositor route still opens them, because it owns the full desktop-entry
launch facility.

The item context menu offers **Open**, **Get Info** and **Show Desktop Entry
File** (Ctrl+Shift+E). Get Info shows the name, description, category, raw XDG
categories, the planned command (display only) and the desktop-entry file
path. Show Desktop Entry File opens the entry's folder and selects it there.
Application items are not files, so they cannot be cut, copied, pasted,
renamed, moved, trashed, bookmarked or dragged, and nothing can be created in
the place. **Keep in Dock**, **Add to Desktop**, **Open in New Workspace**,
sorting by recently used, and dragging an application to the dock or desktop
are not offered yet, because no public boundary for them exists; ADR-0262
lists what each one waits for. Dropping files on an application to open
them with it belongs to Open With (W10). The documents launch contract above
is unchanged.

The Applications behaviour lives in the model and controller layer:
`ApplicationsController` (catalog, rows, Get Info, launch policy),
`ApplicationsListing` (pure projections), the `ApplicationsDirectoryLister` and
`ApplicationsFileLauncher` decorators around the navigation's own seams, and
`ApplicationsPlaceOrder`. The views receive ordinary rows, so the QindaTK views
planned in W11 inherit the place without re-implementing it.

Launched with `--choose-application`, the window opens directly into
Applications as a workspace picker (ADR-0165). A banner explains the choice.
Opening an item hands it to the compositor instead of starting it, so
terminal and D-Bus-activatable applications can also be chosen. A rejected
choice stays in the picker with the reason. The picker window closes when the
compositor swaps the launched application into the layout.

## First-class network locations

S6 turns the Network place from a hint into a destination. Three decisions
own it: [ADR-0194](../adr/0194-saved-network-locations-are-canonical-addresses.md)
for what a saved location is,
[ADR-0195](../adr/0195-one-owner-per-transfer-and-a-queue-for-the-network.md)
for who runs a transfer, and
[ADR-0196](../adr/0196-network-sign-in-belongs-to-the-platform.md) for who
asks for a password.

### Saved locations

A saved network location is **a display name, a canonical address, and a flag
for whether it appears in the Places sidebar** — nothing else. The inventory
is `network-locations-v1.json`, beside `bookmarks-v1.json` under
`$XDG_STATE_HOME/qindaqt-file-manager`, written through the shared
symlink-refusing, size-bounded, atomically committed `StateFile` primitive
that the bookmark inventory also uses (ADR-0090). At most 64 locations, at
most 64 KiB.

```json
{"version":1,"locations":[
  {"name":"Storage (desktop)","url":"sftp://qinda/mnt/storage","showInPlaces":true}
]}
```

- A record's identity **is** its canonical address, so saving the same folder
  twice updates one card instead of adding a second.
- Every address entering or leaving the store must survive
  `NetworkLocation::canonicalize()` unchanged: lower-cased `smb`/`sftp`, a
  non-empty host, no `.` or `..` segment, and no userinfo. A record that does
  not makes the whole inventory `Malformed` — a partly-loaded inventory would
  quietly lose a location the user saved.
- The reader demands an exact key set for the document and for each entry, so
  an inventory written by a newer schema is refused rather than
  half-understood. New fields mean `network-locations-v2` and a migration.
- `NetworkLocationsController` publishes `locations`, the `placesLocations`
  subset, a typed `storeError`, and the last `requestError`. A refused write
  never changes the published list, so the sidebar can never show a location
  the next launch would not find.

### The Network hub and Connect to server

The Network place (and `go.network`, Alt+N) opens `NetworkHub.qml`: the saved
locations as cards with Open and Forget, a **Connect to Server…** button
(`network.connect`, Ctrl+Shift+S), and a plain statement of how signing in
works. Opening a location hands its address to
`NavigationController::navigateTo()` exactly as a bookmark hands over a path,
so an unreachable server lands on the ordinary navigation state pane instead
of a second error channel.

`ConnectToServerDialog.qml` collects a type (`sftp` or `smb`), a server, an
optional port, a folder, an optional name, and the sidebar toggle. It
validates nothing itself: `buildNetworkLocation()` — pure policy, no I/O, no
host resolution, no environment — turns that text into one canonical record or
one refusal, and the refusal is shown verbatim. An empty name is derived from
the address (`/mnt/storage` on `qinda` reads as "storage on qinda").

The dialog has **no user-name, password, or credential-source field**, and a
record carries no user name. That is ADR-0196: gnome-keyring is the session's
only Secret Service provider and QindaQt code never reads or writes collection
contents, so the file manager collects no credential and stores no secret.
Authentication happens inside the KIO job, through the platform's standard UI
delegate (ADR-0151) and its own remember option. For `sftp` that usually means
no prompt at all, because KIO's worker uses libssh and honours `~/.ssh/config`
and the ssh agent.

### Who runs a Copy To or a Move To

The owner of a transfer is decided once, in C++, from the exact pair of source
list and destination, by `TransferRouter::route()` — pure policy, no I/O, no
stat, no server contact. `MutationDialogs.qml` asks it and dispatches to
whichever owner it names.

| Route | Owner | When |
| --- | --- | --- |
| `local` | `MutationController` | no endpoint is on the network |
| `remote-child` | the ADR-0155/0156 path | exactly one source, both endpoints on one network authority |
| `queue` | `TransferQueueController` | anything else involving a network endpoint |
| `refuse` | nobody | an unusable endpoint, or a destination inside its own source |

Because the router can never name `remote-child` for more than one source, the
destination dialog may now open on a remote multi-selection: the ADR-0155/0156
one-child contract is enforced where the dispatch happens rather than by
refusing to open a dialog. An absolute local path normalizes to `file://`; a
relative or uncleaned path is refused rather than repaired, because
`/home/x/../etc` is not the folder the user typed.

### The transfer queue

`TransferQueueController` owns order, dispatch, and every piece of visible
state for network transfers; the injected `TransferWorker` owns the platform
job.

- One source becomes one item, so a ten-file selection is ten items and a
  single failure retires only its own item.
- **Exactly one item runs at a time.** A saturated link makes concurrent
  transfers slower rather than faster, and one running job keeps the
  platform's credential prompts sequential.
- Pause, resume, cancel, and cancel-all belong to the user; the queue never
  retries by itself. Cancel-all retires every live item before dispatching
  anything, so it cannot hand the platform work the user just asked to stop.
- An item that is not Queued, Running, or Paused has been retired, and a later
  worker result for it is dropped. That fence is what makes cancel reliable: a
  quiet KIO kill deliberately delivers no result at all.
- A confirmed success emits `transferCommitted(destinationFolder)`; the window
  re-reads the listing only when that is the folder on screen. The queue never
  navigates or refreshes.
- Only the *destination* travels in that signal, so a queued **move** leaves the
  source folder's listing stale: after a local-to-remote move the local folder
  keeps showing the moved entry until the user navigates away or refreshes. The
  single-child remote-move path does refresh the browsed folder, so the two move
  paths differ here. Carrying the source set alongside the destination is the
  repair; it is not in this slice.
- `TransferQueueBanner.qml` shows what is running, its progress, how many are
  waiting, and Pause/Resume and Cancel all; a refusal gets its own banner.

`KioTransferWorker` implements the seam on `KIO::copy()`/`KIO::move()` and
re-proves the boundary itself, ahead of the router: each endpoint must be a
canonical `smb`/`sftp` URL or an absolute local `file://` URL, neither may
carry userinfo, and at least one must be on the network — a purely local
transfer must never reach KIO from here, because `MutationController` has
identity checks this worker has not.

Overwrite and conflict resolution stay with KIO's standard UI delegate
(ADR-0151); nothing in the queue silently overwrites. Drag-and-drop across the
boundary is deliberately **not** included: the drop pipeline is the
identity-checked local mutation one and has no network authority.

### Nearby servers

Discovery is **opt-in**: it runs only while the "Look for servers on this
network" preference is on, and it is off by default. Opening the Network hub
does not start it, because browsing is a network activity the user chooses.

`AvahiServiceDiscovery` speaks `org.freedesktop.Avahi.Server` on the system
bus, where the already-installed `avahi-daemon` lives. It browses
`_sftp-ssh._tcp` and `_ssh._tcp` (both become `sftp` — KIO's sftp worker
speaks SSH) and `_smb._tcp`. **NFS is deliberately not browsed**: the ADR-0137
allowlist cannot open an NFS location, and a row that cannot be opened is
worse than no row.

One machine is one row. Announcements are reference-counted against the
canonical `scheme://host[:port]` identity that `NetworkLocation::canonicalize()`
accepts, so the ten announcements one laptop makes across `wlan0`,
`tailscale0` and `lo` collapse into a single entry that disappears only when
the last interface withdraws it. A default port is never carried.

Discovery is advisory. Opening a nearby server hands its address to
`NavigationController::navigateTo()` exactly as a saved location does — same
allowlist, same state pane on failure, same platform credential prompt. "Save"
opens the Connect-to-server dialog pre-filled instead. A bus that cannot be
reached or a browser that fails is reported, because silence is
indistinguishable from "there is nothing here"; a platform with no discovery
provider hides the section rather than showing an empty one.

### Preferences

`Ctrl+,` (or File ▸ Preferences) opens a separate non-modal window with four
pages. Its nine settings live in `preferences-v1.json`, in the same app-local
state directory as the bookmarks and the saved locations, over the same
`StateFile` primitive.

| Page | Settings |
| --- | --- |
| General | default view mode, show hidden files |
| Views | sort column, sort order, icon size, folders before files |
| Network | look for nearby servers, default Connect-to-server scheme, what mounting at login costs |
| Trash | ask before moving items to Trash |

This closes the [ADR-0090](../adr/0090-keep-file-manager-bookmarks-app-local.md)
deferral that left sort, hidden visibility, view mode and zoom session-local
pending a Settings1 schema decision: they are one application's view
preferences, nobody else reads them, and Settings1 rejects a whole snapshot on
one unknown key.

Three rules make the file safe to trust.

- **Every preference does something**, and every default is what the
  application already did before preferences existed — so a first run behaves
  exactly as it used to.
- **Exact, not tolerant**: an unknown key, a wrong type, or a value outside its
  documented set refuses the whole document and leaves the defaults standing.
  An icon size is refused rather than snapped, because a view at a size off
  the zoom ladder is one the zoom controls can never leave.
- **A refused write changes nothing visible**, so the window always shows what
  the next launch will read.

`PreferencesController` applies nothing itself; `PresentationDefaults.qml`
binds it to `NavigationController`, so a change reaches the window already on
screen without either class depending on the other. That apply step is
idempotent on purpose: `setSortColumn()` *flips* the direction when called
with the column that is already active, and a needless re-sort moves the view
under the user's selection.

Only the recoverable home Trash may skip its confirmation. Empty Trash is
permanent and always asks.

### Mount at login

An SFTP location can carry a "Mount under ~/Network at login" knob
(ADR-0199). Turning it on writes **one systemd user `.mount` unit** and asks
the user's own service manager to notice it. There is no mount daemon, no
retry loop and no credential: `sshfs` authenticates exactly as `ssh <host>`
does, because a saved location is userinfo-free by construction.

```ini
[Mount]
What=qinda:/mnt/storage
Where=/home/cabewse/Network/Storage
Type=fuse.sshfs
Options=_netdev,reconnect,ServerAliveInterval=15,ServerAliveCountMax=3,idmap=user
```

- The unit file name is systemd's path escaping of `Where=`, computed rather
  than guessed — systemd resolves a `.mount` unit's mount point from its own
  name, so the two must agree exactly.
- **Ownership is a prefix**: the manager only ever touches units whose name
  begins with the escaping of `<home>/Network/`. A unit the user wrote by hand
  in the same directory is never read, rewritten, or removed.
- The knob is **sftp only** — sshfs is the one FUSE filesystem it knows and
  the one that needs no root — and the store refuses to record it on anything
  else.
- Synchronising is idempotent: an unchanged unit is not rewritten and systemd
  is not asked to reload.

Turning the knob on is what makes `net-fs/sshfs` a runtime dependency, for
that user on that machine. QindaQt does not install it; a missing sshfs shows
up as a failed mount in the user's journal, and the Preferences copy says so.

The saved-location inventory is `network-locations-v2` to carry the knob. A v1
inventory is read once, reported as migrated, and rewritten as v2 with the
knob off; the v1 file is left in place so a downgrade loses nothing.

### Focused rows

`qindaqt.file-manager-network-locations-store`,
`qindaqt.file-manager-connect-request`,
`qindaqt.file-manager-network-locations-controller`,
`qindaqt.file-manager-transfer-router`, `qindaqt.file-manager-transfer-queue`
(against a recording worker double — no KIO, no network, no filesystem), and
`qindaqt.file-manager-kio-transfer-worker` (the realm boundary and the
retained KIO UI delegate through a job-creation seam);
`qindaqt.file-manager-avahi-discovery` and
`qindaqt.file-manager-discovery-controller` (the browse/resolve/collapse/retire
state machine with canned D-Bus replies — no bus, no daemon, no network, and
the visible model over a fake backend);
`qindaqt.file-manager-preferences-store` and
`qindaqt.file-manager-preferences-controller`; and
`qindaqt.file-manager-mount-unit` and `qindaqt.file-manager-mount-manager`
(unit text and name, write/enable/retire/idempotence, and the ownership
prefix — **no row runs `systemctl`**, because one that did would touch the
developer's own user manager).

`qindaqt.file-manager-network-hub-ui` drives the production
coordinator → `Main.qml` route: the hub renders (a real parent chain to the
window and a positive painted size, not merely `visible`), Connect to Server
saves and opens, a refused address explains itself and saves nothing, a
committed transfer refreshes only the folder on screen, and Preferences opens,
renders every page, and a real click on a real control reaches both the folder
on screen and the file on disk.

The installed-package probe `--check-ui-contract` additionally requires the
hub, its list and Connect button, the dialog and its fields, both transfer
banners, the Nearby section, and the preferences window with all four pages.

## Ownership, lifetime, and failures

- `DirectoryEntry`/`ListingResult`/`LaunchResult` (`model/file_manager_types.h`,
  `model/launch_intent.h`) are plain values. They perform no I/O.
- `ListingOrder` (`model/listing_order.h`) is the pure sort policy shared by
  the controller and the QML header row. Its default value reproduces the
  original directories-first, case-insensitive-name order exactly.
- `NavigationHistory` (`model/navigation_history.h`) owns only back/forward/
  current-path bookkeeping plus pure lexical parent and breadcrumb-segment
  computation. It performs no I/O and displays no UI, mirroring the Text
  Editor's `DocumentState` separation of policy from I/O.
- `DirectoryLister` is an injected synchronous boundary.
  `LocalDirectoryLister` performs one bounded local `QDir` read (at most
  `LocalDirectoryLister::maximumEntries`, currently 20000, entries; beyond
  that `ListingResult::truncated` is set rather than silently dropping
  entries or blocking indefinitely, and the ready-state list shows the
  bounded-entry count notice) and returns a typed `ListingError`
  instead of throwing.
- `FileLauncher` is a second injected synchronous boundary.
  `DesktopFileLauncher` performs the bounded validation and dispatch described
  above; `DesktopFileLauncher::validateRegularFile()` is a public static seam
  so every pre-flight rejection has a deterministic, environment-independent
  test.
- `KioFuseRemoteFileOpener` (`network/kio_fuse_remote_file_opener.h`) is the
  production remote opener (ADR-0157) and the only remote-write owner: it
  re-validates the scheme/no-userinfo boundary before any contact, resolves
  the canonical URL through the session KIOFuse D-Bus service, opens the
  returned local path through `KIO::OpenUrlJob` with KIO's standard UI
  delegate, and falls back to the ADR-0152 direct open when the facility
  cannot answer. It owns no downloader, sync engine, mount, or credential
  authority — KIOFuse's daemon lifetime belongs to the platform — and each
  `open()` is an independent, monotonically keyed operation whose pending
  watcher and job die quietly with the object.
- `NavigationController` (`model/navigation_controller.h`) is a GUI-thread
  `QObject` that owns one injected `DirectoryLister` and one injected
  `FileLauncher` for its whole lifetime. It keeps the raw listing and
  re-derives the visible (hidden-filtered, sorted) snapshot when
  presentation settings change, without re-reading the directory. It
  publishes complete navigation/listing/launch-error state through Qt
  properties and never shows a dialog, chooses a selection, or retries on
  its own. Entry snapshots carry preformatted `sizeText`/`kindText`/
  `modifiedText` so QML delegates stay presentation-only.
- `StateFile` (`model/state_file.h`) owns the one app-local state-file
  primitive: the Linux `openat`/`O_NOFOLLOW` directory walk, the regular-file
  check, the size bound, and the same-directory `QSaveFile` commit. It owns no
  schema, no JSON, and no human-readable message; each store composes one and
  translates its `Error` into its own typed error and diagnostics.
- `BookmarksStore` (`model/bookmarks_store.h`) owns the versioned, bounded,
  symlink-refusing bookmark file beneath `$XDG_STATE_HOME` with atomic
  same-directory replacement (ADR-0090), over that primitive.
  `PlacesController` owns the fixed places list, bookmark add/remove/dedup/cap
  policy, and typed store-error publication; it never navigates or lists
  directories.
- `NetworkLocationsStore` (`network/network_locations_store.h`) owns only the
  `network-locations-v1` inventory over the same primitive (ADR-0194). It
  never discovers HOME, contacts a server, resolves a host, or touches a
  credential store. `NetworkLocationsController` owns the saved-location list
  for one window and publishes complete snapshots plus typed store/request
  errors; it never navigates or lists a folder, and a refused write leaves the
  published list untouched.
- `buildNetworkLocation()` (`network/connect_request.h`) is the one place
  Connect-to-server dialog text becomes a saved location. Pure policy: no I/O,
  no host resolution, no environment, so every refusal is deterministic.
- `TransferRouter` (`network/transfer_router.h`) is pure routing policy: it
  decides *who* runs a Copy To / Move To, never whether the source exists.
- `TransferQueueController` (`network/transfer_queue_controller.h`) owns the
  order, the one-at-a-time dispatch, per-item state, pause/resume/cancel, and
  the typed refusal surface for network transfers (ADR-0195). It lists no
  folder, shows no dialog, resolves no conflict, and never retries on its own.
  `TransferWorker` is the injected platform seam; `KioTransferWorker` is the
  production implementation on `KIO::copy()`/`KIO::move()`, which re-proves the
  realm boundary independently and owns each job's lifetime (destruction kills
  a pending job, and its prompt, quietly).
- `MutationBackend` (`mutation/mutation_backend.h`) is the synchronous,
  worker-thread operation seam. `LocalMutationBackend` owns create, rename,
  copy, and move policy; `HomeTrash` owns only Trash/restore/empty behavior;
  `DeviceResolver` isolates device identity for deterministic cross-device
  refusal tests.
- `MutationController` owns backend lifetime, worker scheduling, progress,
  cancellation, typed presentation state, serialized batch execution, and
  one-level recovery. It never lists folders, shows dialogs, or acquires
  shell/service authority. Foreign-path batches (clipboard/DnD payloads
  without a listing snapshot) are stat'ed at dispatch and capped at
  `MutationController::maximumForeignPaths`.
- `ClipboardController` (`model/clipboard_controller.h`) owns only clipboard
  policy: the own cut/copy snapshot, foreign payload adoption, paste/drop
  destination validation, and the paste-into-self guards. All mutation goes
  through the injected `MutationController`; it never touches the filesystem
  itself.
- `SearchController` (`model/search_controller.h`) owns one bounded
  recursive-search worker. Results cross to the GUI thread by value in one
  token-fenced signal; cancel/restart/destruction invalidate the token before
  joining and disposing the worker, so a queued late delivery is dropped
  rather than dereferencing disposed state.
- `EntryPropertiesController` (`model/entry_properties.h`) owns the
  properties-dialog state and the bounded total-size worker with the same
  generation-fenced disposal contract. It never mutates the filesystem.
- `fileManagerActionCatalog()` contributes the closed mutation, view, edit,
  and navigation action set to AppShell, projected from the public menu
  catalog ([below](#public-menu-catalog)). `ApplicationCoordinator` transports
  activation and close requests but never examines a path or decides whether
  an operation is recoverable.
- `composeFileManagerMenuExport()` is the application composition boundary. It
  lends the primary window, coordinator, and session-bus connection to the
  opt-in AppShell exporter; it contains no filesystem or shell authority.
- QML (`ui/Main.qml` and its `Toolbar`/`Breadcrumb`/`LocationBar`/
  `PlacesSidebar`/`EntrySelection`/`EntryList`/`EntryGrid`/`StatePane`/
  `StatusBanners`/`NetworkHub`/`NetworkLocationCard`/`ConnectToServerDialog`/
  `TransferQueueBanner` collaborators) owns only presentation: layout, keyboard routing to the
  controller's invokable methods, accessible names/roles, and the
  presentation-owned selection. It never lists a directory or launches a file
  itself.

All expected errors cross the lister/launcher/mutation boundaries as typed
values plus bounded human-readable diagnostics. There is no D-Bus authority,
shell-private dependency, global worker pool, or exception-based failure
channel. The one mutation worker and its backend are private implementation
details with constructor-visible ownership. The optional menu transport is a
borrowed AppShell adapter, not File Manager domain authority.

The `model/**` and `mutation/**` C++ headers and build target are private
implementation surfaces and are not installed or ABI-stable. The executable
name, desktop ID, folder-launch-argument contract, and documented action
object names/shortcuts form the compatibility surface.

## Public menu catalog

File Manager's menu vocabulary — the File, Edit, View, and Go titles and every
action's id, label, description, and shortcut — is a small public catalog of
values, `public/file_manager_menu_catalog.h` (target
`qindaqt_file_manager_menu_catalog`, Qt Core/Gui only, exporting only
`public/`). `fileManagerActionCatalog()` projects it into AppShell actions,
and the shell's desktop menu (the File Manager's menu shown in the global menu
while no application is active,
[ADR-0260](../adr/0260-show-the-file-managers-menu-when-no-application-is-active.md))
projects the same values, so the desktop and a File Manager window cannot
drift apart. Neither consumer spells a shared string. The catalog also names
the application's short title ("File Manager", the desktop entry's
GenericName), desktop entry id, and icon, and one command File Manager's
window menu does not offer yet, `file.new-window` ("New File Manager
Window", no shortcut until a handler honours one). Standard-key shortcuts
(Undo, Cut, Copy, Paste, Zoom) stay platform-resolved exactly as before.

## Public Desktop file boundary

`QindaQt::Apps::FileManager::Desktop::FileBoundary`
(`public/desktop_file_boundary.h`) is the one narrow, stateless seam through
which Desktop-owned code (and, later, the network worker's URL/job
integration) reaches File Manager's local-filesystem authority. It is the
only File Manager surface Desktop may depend on. The dedicated
`qindaqt_file_manager_desktop_boundary` target contains that seam and its local
listing/launch/mutation implementation without AppShell or KIO, so the shell's
runtime closure cannot inherit application-only shared libraries. `model/**`,
`mutation/**`, and `app_shell/**` remain private controllers (see [Module
boundaries](../architecture/module-boundaries.md)).

- `listLocalFolder(absolutePath)` composes `LocalDirectoryLister` and returns
  the same `ListingResult`/`DirectoryEntry` values File Manager's own
  navigation uses: one bounded synchronous local read (at most
  `LocalDirectoryLister::maximumEntries` entries), a typed `ListingError`
  instead of a thrown exception, and identity fields (device, inode, size,
  modification time, mode) a caller can hand straight to a mutation request
  without a second stat.
- `launchLocalFile(absolutePath)` composes `DesktopFileLauncher` and performs
  the identical validate-then-`QDesktopServices::openUrl` local launch
  contract documented above, returning a typed `LaunchError` on any
  pre-flight rejection or declined handler.
- `openLocalFolder(absolutePath, listed, programCandidates, start)` opens one
  listed local folder in QindaQt File Manager.
  - **Identity.** The path must still be the object `listLocalFolder`
    reported. `ListedIdentity` carries its device and inode under the same
    `lstat` semantics, so a symlink entry is identified by the link.
  - **Target.** The path must resolve once to a readable, enterable
    canonical directory.
  - **Program.** The first absolute, executable
    `fileManagerProgramCandidates()` entry is used: the running application's
    sibling `qindaqt-file-manager`, then the `PATH` lookup result.
  - **Launch.** That program is started detached, with exactly the canonical
    directory as its single argument. No shell, URL handler, or default
    `inode/directory` association is involved.
  - **Refusals.** A missing, dangling, or relative path (`NotFound`), a
    replaced object (`Replaced`), a non-directory (`NotDirectory`), an
    unreadable directory (`Unreadable`), no installed program
    (`NotInstalled`), or a failed start (`LaunchRefused`) returns a typed
    `FolderOpenError`. It never falls back to another folder or program. Only
    `LaunchRefused` happens after a start attempt.
  - **Success** means the process started. File Manager revalidates its
    folder argument (exit 4 otherwise), and its later exit is not observed.
- `createLocalMutationController(parent)` composes one `MutationController`
  over a `LocalMutationBackend` rooted at the same `$XDG_DATA_HOME/Trash`
  File Manager's own `main.cpp` wires (ADR-0064), so Desktop-initiated
  create/rename/copy/move/Trash/restore requests are identity-checked and
  behave identically to File Manager's own. This composes already-accepted
  controller/backend wiring; it adds no new mutation policy or architecture.

Every member is GUI-thread only and performs no I/O beyond the bounded local
reads/writes the composed classes already document. `listLocalFolder` and
`launchLocalFile` are synchronous and stateless; `createLocalMutationController`
returns a GUI-thread-confined `QObject` that owns exactly one worker thread
and at most one in-flight operation, publishing bounded progress and typed
failure through its existing properties/signals. The caller owns the returned
controller's lifetime (via `parent`, or by keeping the `unique_ptr` alive) for
as long as an operation may be in flight. The boundary resolves only local
absolute paths; portal, network, and mount locations are out of scope and
remain later slices. The focused row is
`qindaqt.file-manager-desktop-file-boundary`, covering a real temporary-file
listing success/failure case, launch pre-flight rejections, and an end-to-end
Trash operation against fixture identity obtained from `listLocalFolder`.

## Stock-controls presentation and accessibility boundary

Per [ADR-0116](../adr/0116-build-bundled-applications-on-stock-qt6.md), every
`ui/` file imports only `QtQuick`, `QtQuick.Controls`, and `QtQuick.Layouts`.
Colors, fonts, and control metrics come from the Qt platform theme: QML
components read their `Control` root's `palette`, and `main.cpp` publishes no
QST generation, loads no theme catalog, and binds no appearance controller.
`--theme` and `--theme-directory` remain accepted as deprecated no-ops so
external harnesses that still pass them keep working; `--check-theme` is
removed. Entry icons resolve through the application-owned
`image://theme-icons/` provider, which asks the active `QIcon` theme and
returns a transparent placeholder rather than a warning when a name is
missing.

The folder list and grid, sort headers, breadcrumb buttons, location field,
places and bookmark rows, toolbar buttons, and every state card expose
accessible names/roles/descriptions through `Accessible.role`/`.name` on each
QML item and the stock Qt Quick Controls accessibility contracts. List entries
additionally state whether they are a folder or a file in their accessible
name so a screen reader user does not have to rely on icon shape or color
alone.

## Desktop integration and verification

The visual revision adds focused local-decoder and asynchronous-provider tests
for resource admission, corrupt input, cancellation, two-job concurrency,
generation fencing, cache identity rechecks and listing refresh. A test-only
`qindaqt_file_manager_visual_probe` instantiates production QML and real local
controllers, creates its own browsing fixture, and saves an offscreen capture.
It accepts source root, theme ID (mapped onto a light/dark platform color
scheme), output path, width and height; an optional
last argument enables reduced transparency. Output belongs under ignored build
or cache directories. This probe does not add a production screenshot API.


`org.qindaqt.FileManager.desktop` registers the ordinary Wayland application
for `inode/directory` with one `%u` local-folder argument. Multiple folder
arguments, and a positional argument that is not a folder, are both rejected
rather than silently opening the wrong location. Positional validation occurs
before any QML or icon setup so the documented exit and diagnostic remain
stable in a minimal or deliberately sanitized package environment.

The focused selector is:

```sh
ctest --test-dir build/dev -L file-manager --output-on-failure
```

It covers `NavigationHistory`'s back/forward/up/breadcrumb truth (including
root and malformed-path edge cases), `LocalDirectoryLister`'s sort order,
hidden/symlink/permission-denied/missing/not-a-directory/empty listing
behavior, `DesktopFileLauncher`'s missing/directory/dangling-symlink/
unreadable pre-flight rejections and canonical-target resolution,
`NavigationController`'s dispatch/history/status-mapping/selection-restoration
contract against injected fakes plus its S2 sorting, hidden-filter, formatted
field, and view-mode behavior, the pure `ListingOrder` policy across every
column and direction, the `BookmarksStore` round-trip/bounds/symlink-poison
contract and `PlacesController` policy, desktop metadata, and CLI arity/
argument validation. The package row stages only the `FileManager` component
in a clean disposable prefix. That component carries the executable, its
desktop metadata, and the AppShell/Controls/Tokens backing shared libraries
the executable's dynamic dependency chain still needs; it deliberately carries
no importable QindaQt QML module metadata or QML sources and no per-app theme
catalog (ADR-0116), and the row asserts both shapes. With ambient QML and
library paths cleared, the gate rejects an executable that embeds the build
QML directory and constructs the real File Manager QML root offscreen through
`--check-qml-root` — with the deprecated `--theme`/`--theme-directory` options
present to prove they stay accepted no-ops — before exiting deterministically.
The executable's library lookup is relative to its installed location, so the
same component remains usable after staging or relocation.

S1 adds mutation, Trash, controller, action-catalog, UI-contract, UI-action, and
boundary-policy rows. S2 extends them: the UI-contract row also requires the
sort headers, location field, view/hidden toggles, grid view, and sidebar
object names; the UI-actions row drives the hidden-toggle round trip, a
header-clicked size sort with direction toggle, and a multi-item batch Trash
through the production action seam against disposable fixture seeds. The batch
unit rows prove sequential ordering, per-item identity recheck,
stop-on-first-failure with the "Completed N of M" diagnostic, stale-selection
rejection before any dispatch, mid-batch cancellation, and that a completed
batch arms neither undo nor the restore token. The first-party export slice adds a private-bus row that
runs the real File Manager against the production shell global-menu
composition, injects its exact child PID and window id, activates New Folder
once, and proves owner exit clears the shell menu. Separate real-process
variants inject a mismatched PID and a mismatched registrar window ID; each
keeps the shell facade unavailable and records zero activation. Fixture trees exercise Unicode/control-character names,
overlong rejection, permissions, collision, before/during-operation vanishing,
identity change, nested and root symlink poison, cancellation cleanup, Trash
round trips, an in-flight nested-directory swap, unique names, restore
collision, orphan-payload allocation, vanished restore parents, empty Trash,
an injected cross-device refusal, and a racing destination writer.
Both offscreen rows run under `QT_FATAL_WARNINGS=1` with
host display and session-bus variables removed. The
boundary checker rejects dependencies on shell/services, D-Bus, KWin,
LayerShell, desktop launch, or QML from the mutation module and proves itself
against planted poison. All Trash roots live below disposable fixture/build
directories; no row reads or mutates the user's home Trash. Bookmark store
rows likewise live below `QTemporaryDir` roots and never touch the real
`$XDG_STATE_HOME` inventory.

## Roadmap

- **S2 (this slice, landed)** — editable location bar, multi-select with
  serialized batch mutation, configurable sorting with size/kind/modified
  columns, hidden-file toggle, list/grid view switch, places/bookmarks
  sidebar with app-local persistence (ADR-0090).
- **S3 (partly landed)** — daily-use completion: the file clipboard
  cut/copy/paste, drag-and-drop, bounded recursive search, and the properties
  dialog are delivered. Still open: a preview pane, an open-with chooser
  (requires widening ADR-0029's launch contract through a new ADR), optional
  permanent deletion, and refinement beyond the shipped public Controls icon
  boundary (ADR-0111).
- **S4** — volumes: mount enumeration and per-volume Trash (supersedes the
  ADR-0064 deferral with its own ADR; coordinate polkit/udisks boundaries
  through the Program Manager thread first).
- **S5 (read-only browsing landed)** — `smb://`/`sftp://` folder browsing
  through the injected, asynchronous `NetworkDirectoryBackend` seam and the
  production `KioNetworkDirectoryBackend` adapter (ADR-0137), with ordinary
  KIO authentication prompts (ADR-0151), remote regular-file opening
  through `KIO::OpenUrlJob` behind the injected `RemoteFileOpener` seam
  (ADR-0152); because KIO's standard delegate prompts with QWidgets (its
  Open With dialog when no application is associated, plus credential and
  message boxes), the File Manager process runs a QWidget-capable
  `QApplication` composed through the shared factory in
  `runtime/file_manager_application.h`, while the UI itself stays Qt Quick
  (ADR-0116). Same-folder remote Rename runs through `KIO::rename()` behind
  the injected `RemoteRenamer` seam (ADR-0153), and New Folder creates one
  validated child directory through `KIO::mkdir()` behind the injected
  `RemoteFolderCreator` seam (ADR-0154). Copy To sends one listed child to
  a validated remote destination folder through `KIO::copy()` behind the
  injected `RemoteCopier` seam (ADR-0155), and Move To moves one listed
  child the same way through `KIO::move()` behind the injected `RemoteMover`
  seam (ADR-0156) -- a destructive operation, so a confirmed success always
  refreshes the visible folder (the source left it) and the pre-dispatch
  listed-child/same-authority/no-userinfo checks are the last line of
  defense before KIO. Remote write-in-place (ADR-0157): the stock opener
  is `KioFuseRemoteFileOpener`, which validates the same scheme/no-userinfo
  boundary before any contact, asks the session's standard KIOFuse service
  (`org.kde.KIOFuse.VFS.mountUrl`, the same client `libKF6KIOGui` embeds)
  to expose the SAME canonical remote URL as a local FUSE path, and opens
  that local path through `KIO::OpenUrlJob` with the standard UI delegate
  retained; the desktop handler edits an ordinary local file and KIOFuse
  owns writing the saved bytes back to the mounted URL on close. Where the
  facility cannot answer (no session bus, mount error, malformed reply) the
  opener falls back to the ADR-0152 direct remote open, so remote open
  never regresses without KIOFuse; opens stay fire-and-forget (no busy
  state, no listing refresh, no shared Cancel owner), each keyed by its own
  monotonic identity, and destruction kills pending watchers and jobs
  quietly. Still open: mount-based volume
  access (S4); portal locations remain out of scope.
- **S6 (this slice, landed)** — first-class network locations: the
  `network-locations-v1` saved-location inventory, the Network hub page and
  Connect-to-server dialog (ADR-0194), and the transfer queue with its routing
  policy and `KIO::copy()`/`KIO::move()` worker, which copies and moves
  between the local machine and a network location in either direction
  (ADR-0195). Sign-in stays the platform's: QindaQt collects no credential and
  stores no secret (ADR-0196), so a QindaQt credential-entry UI is not a
  deferral but a decided non-goal. Still open in this area: server discovery
  over Avahi, mount-at-login under `~/Network/<name>`, a preferences window, a
  per-location remote user name (a `network-locations-v2` field), a
  QindaQt-owned conflict dialog, and drag-and-drop across the local/network
  boundary.
- **S7 (this slice, landed)** — the rest of first-class network locations:
  opt-in Avahi discovery of servers the file manager can actually open
  (ADR-0200), the `preferences-v1` store and the Preferences window, which
  closes the ADR-0090 session-local deferral (ADR-0198), and the per-location
  mount-at-login knob that writes one systemd user `.mount` unit, with the
  inventory bumped to `network-locations-v2` and a v1 migration (ADR-0199).
  Still open in this area: a per-location remote user name, in-place versus
  copy-on-open, a QindaQt-owned conflict dialog, drag-and-drop across the
  boundary, remote thumbnails, and a connection-timeout/retry policy.

## Bounded deferrals

- Sort column/direction, hidden visibility, list/grid mode, icon zoom, and
  filename filtering are session-local; persisting them is a Settings1 schema decision deferred per
  ADR-0090.
- Batch operations are not covered by undo or Restore Last (one-level,
  single-item recovery is unchanged from S1).
- Permanent deletion outside confirmed Empty Trash, per-volume Trash, mounts,
  additional preview formats, portal-mediated paths, and open-with remain
  explicit later outcomes (S3–S5). A QindaQt credential-entry UI is no longer
  among them: ADR-0196 decides that sign-in belongs to the platform.
- Batch transfers across the local/network boundary are landed as the S6
  transfer queue (ADR-0195). The single-child same-authority remote Copy To
  and Move To (ADR-0155/0156) keep exactly their reviewed case, so a batch
  within one authority runs through the queue rather than through them.
  Remote write-in-place (ADR-0157) still covers one regular file opened
  through the desktop handler; batch remote writes remain deferred with it.
- Overwrite and conflict resolution during a queued transfer are KIO's
  standard dialog, not a QindaQt one, and the queue carries no per-item
  conflict state yet.
- Mount at login writes and registers a unit; **an actual mount has not been
  observed**. No focused row runs `systemctl`, and `net-fs/sshfs` is not
  installed on the laptop, so proving a mount needs an install and a login and
  belongs to whoever cuts the package.
- Discovery shows what a machine advertises. It does not probe, does not
  verify that a server will accept a connection, and has no periodic refresh:
  a browser left running relies on Avahi's own announcements.
- One-level undo/restore is process-local and deliberately not a durable
  recovery journal. Single-item copy has no undo; users can trash its
  destination in a separate confirmed action.
- The offscreen UI rows prove construction, stable action/object identity,
  fatal-warning cleanliness, and the fixture-local identity-carrying mutation
  path. Nested screenshots and whole-application
  assistive-technology qualification remain later release evidence.

The browsing-comfort regressions include `qindaqt.file-manager-name-filter`,
`qindaqt.file-manager-icon-zoom`, `qindaqt.file-manager-viewport`, and
`qindaqt.file-manager-browsing-ui`. The last loads real File Manager controllers
and production QML with temporary local files, then delivers keyboard and wheel
input at compact, desktop and 1080p sizes under light and dark platform color
schemes. It validates the file clipboard only through the production action seam
(see below); mounted volumes remain explicit S4 work.

The S5 network-browsing rows are `qindaqt.file-manager-network-location`
(allowlist/canonicalization: supported/unsupported schemes, credential and
`..`-escape refusal, parent/breadcrumb/child URL derivation),
`qindaqt.file-manager-navigation-controller-network` (async success/typed
error/stale-generation/URL-mismatch/truncated-listing/local-remote routing/
remote-directory-activation/remote-file-open-through-the-injected-opener/
typed-open-failure-and-error-clearing/remote-rename-dispatch-validation-
refresh-failure-and-stale-discard/remote-create-dispatch-validation-
refresh-failure-and-stale-discard/remote-copy-dispatch-validation-
success-no-refresh-for-other-folders-failure-and-stale-discard, all against
fake injected backends),
`qindaqt.file-manager-kio-network-backend` (the production
`KioNetworkDirectoryBackend`'s own scheme/policy boundary via a
job-creation test seam), `qindaqt.file-manager-kio-remote-opener` (the
production `KioRemoteFileOpener`'s scheme boundary and retained KIO UI
delegate via a job-creation test seam, plus one real open of an
unassociated local file type driven to KIO's standard Open With prompt
under the production application class — the dialog is dismissed
hermetically, so no application is started),
`qindaqt.file-manager-kio-fuse-remote-opener` (the production
`KioFuseRemoteFileOpener`'s scheme/userinfo refusal before any contact,
canonical same-URL mount resolution opened as a local file:// job with the
retained KIO UI delegate, the direct-open fallback on a mount error, a
malformed reply, or a missing facility, bounded no-credential diagnostics,
independent concurrent opens, and quiet destruction with a pending
resolution — all through the faked `createMountCall`/`canResolve`/
`createOpenUrlJob` seams, hermetically: no session bus, KIOFuse daemon,
network, or desktop handler),
`qindaqt.file-manager-kio-remote-renamer` (the production
`KioRemoteRenamer`'s scheme/same-folder boundary and retained KIO UI
delegate via a job-creation test seam),
`qindaqt.file-manager-kio-remote-folder-creator` (the production
`KioRemoteFolderCreator`'s scheme/userinfo boundary and retained KIO UI
delegate via a job-creation test seam),
`qindaqt.file-manager-kio-remote-copier` (the production `KioRemoteCopier`'s
scheme/authority boundary and retained KIO UI delegate via a job-creation
test seam),
`qindaqt.file-manager-kio-remote-mover` (the production `KioRemoteMover`'s
scheme/authority boundary and retained KIO UI delegate via a job-creation
test seam), `qindaqt.file-manager-remote-move-dispatch` (the injected move
dispatch/validation rows: a confirmed success always refreshes the visible
folder because the source left it, failures stay visible without optimistic
display, replacement navigation and direct user cancellation retire the job
with an operation-identity-fenced late result -- a Cancel followed by an
immediate same-listing retry reuses the listing generation, so each accepted
move carries its own monotonic, never-reused operation token, and the old
move's late failure or success can never retire, fail, or refresh the
replacement -- and destruction with a pending move is
safe), `qindaqt.file-manager-remote-copy-guard` (the production
coordinator → Main.qml → MutationDialogs route: a remote multi-selection
fails closed before the destination dialog and the local-only mutation
backend for both Copy and Move, one selected child still routes to the
injected copier or mover, and the
shared Cancel action retires an in-flight remote copy or move through the
injected collaborator with a fenced late result), and `qindaqt.file-manager-mutation-action-binding`
(the coordinator's actual action-enabled state, not just controller fields,
under `NavigationController::remoteActive`: current-folder mutations and
`view.filter`/search disable, while Empty Trash, Undo, and Restore Last stay
available and are instead gated only by the mutation-busy slot; the shared
Cancel action additionally tracks an in-flight remote copy or move, and the
destructive Move action additionally disables while its own move is in
flight). None of
these rows make a DNS lookup, socket connection, or real KIO network request;
the production adapter itself is
otherwise only exercised by construction/linking.

The S3 daily-use rows are `qindaqt.file-manager-clipboard-controller`,
`qindaqt.file-manager-recursive-search`, and
`qindaqt.file-manager-entry-properties`. They prove the own-snapshot cut/copy
interplay with the offscreen clipboard (including the no-ownership-report
regression), copy-vs-move dispatch, cut-paste clearing after commit, foreign
copy-only adoption, drop URL filtering/deduplication, and paste-into-self
refusals; the recursive matcher across depth/hidden/symlink bounds, cancel and
supersede fencing, and restart; and the properties fields, bounded total-size
walk, and stale-worker fencing. The UI-actions row drives `edit.copy`/
`edit.cut`/`edit.paste` through the production QML action seam against
on-disk fixtures, proving menu-armed enabled state, copy-paste commit,
cut-paste move, and post-commit clipboard clearing end to end.
