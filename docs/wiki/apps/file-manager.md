# QindaQt File Manager

`qindaqt-file-manager` is QindaQt's first-party local-directory browser. S0
landed bounded listing, navigation, and regular-file launch. S1 adds local new
folder, rename, copy, same-filesystem move, home Trash, restore, and
empty-Trash operations with cooperative cancellation, progress, typed failure,
and one-level recovery. S2 adds the core browsing surface: an editable
location bar, multi-select with serialized batch operations, configurable
sorting with size/kind/modified columns, a hidden-file toggle, a list/grid
view switch, and a places/bookmarks sidebar persisted in an app-local state
file. The visual browsing revision adds catalog icons and bounded local raster previews. Search, drag-and-drop, per-volume Trash, mounts, and network
locations remain later slices (see the roadmap below).

The durable local-launch choice is recorded in
[ADR-0029](../adr/0029-file-manager-bounded-local-launch.md); the S1 mutation
and Trash authority is recorded in
[ADR-0064](../adr/0064-confine-file-mutation-to-identity-checked-local-authority.md);
the S2 bookmark persistence contract is recorded in
[ADR-0090](../adr/0090-keep-file-manager-bookmarks-app-local.md).
Bounded local previews and public icon composition follow
[ADR-0111](../adr/0111-bound-file-previews-and-consume-public-icons.md).

S2 composes `QindaQt.Tokens 1.0`, `QindaQt.Controls 1.0`, and the public
`QindaQt.AppShell 1.0` window/action/lifecycle boundary. File Manager retains
all navigation and filesystem policy; AppShell owns only the standard menu,
shortcut dispatch, focus reporting, and close-decision protocol (see
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
menu retains the full hierarchy. `Ctrl+L` always reveals the complete path. Forward is also shown above 680 pixels;
its keyboard/menu action remains available at compact widths. Folder Options
holds direct location entry, hidden files, refresh, sorting and Trash recovery.
`Ctrl+L` replaces the breadcrumbs with a themed location field; Enter uses the
same normalized navigation boundary, and Escape restores the breadcrumbs.
Tooltips and accessible labels explain every icon action.

The places sidebar offers fixed places (Home, File System, Trash) and the
user's bookmarks. `Ctrl+D` bookmarks the current folder; each bookmark row
has an icon button to remove it. Places retain both recognizable icons and labels;
the sidebar narrows from 196 to 148 pixels below 680 pixels window width. Bookmarks persist across restarts through
`BookmarksStore` (ADR-0090); a bookmark whose folder vanished simply lands on
the ordinary "missing" state card.

The main pane defaults to a spacious icon grid. Details mode exposes a clickable
sort header (Name, Size, Kind, Modified); metadata columns progressively hide
below the available width, preserving the filename and size. Clicking the active column reverses its
direction; directories sort first by default. Hidden entries (dot names) are
filtered out of the published listing by default; `Ctrl+H` or the toolbar
toggle shows them at their sorted positions, and the status notice reports
the filtered count ("3 hidden"). List mode shows preformatted size, kind, and
modified columns; grid mode shows the same entries as catalog/MIME icons with local image previews. Both modes
share one selection contract. Original folder artwork adds a decorative empty
state at roomy sizes, while compact windows retain the accessible state card.

| Action identity | Shortcut | Meaning |
| --- | --- | --- |
| `go.back` / `navigateBackButton` | `Alt+Left` | Return to the previous folder in history |
| `go.forward` / `navigateForwardButton` | `Alt+Right` | Return to the folder undone by Back |
| `go.up` / `navigateUpButton` | `Alt+Up`, `Backspace` (when the list has focus) | Go to the parent folder |
| `view.refresh` / `refreshButton` | `F5`, `Ctrl+R` | Re-read the current folder |
| `view.focus-location` | `Ctrl+L` | Swap the breadcrumb for the editable location field |
| `view.show-hidden` | `Ctrl+H` | Show or hide dot-name entries (checkable) |
| `view.grid-mode` | `Ctrl+2` | Switch between details and icon presentation (checkable) |
| `edit.select-all` | `Ctrl+A` | Select every visible entry |
| `go.home` | `Alt+Home` | Open the home folder |
| `bookmark.add` | `Ctrl+D` | Bookmark the current folder |
| `entryListView` / `entryGridView` | `Return`/`Enter` | Open the selected entry |

Selection is shared between list and grid views. Ctrl-click toggles individual
files; Shift-click and Shift+arrows select a range; Ctrl+A selects all visible
entries. Plain arrows select the destination, while Ctrl+arrows move only the
focus outline. Grid Up/Down move by a row. Right-clicking an already selected
file keeps the batch selected for the context menu.

Sorting and refreshing keep the same selected files, identified by name,
device and inode. Filtering drops entries that are no longer visible; revealing
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
lists cleanly but has no children presents one accessible
`QindaQt.Controls` `StateCard` instead of an empty or frozen-looking list. A
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
- `NavigationController` (`model/navigation_controller.h`) is a GUI-thread
  `QObject` that owns one injected `DirectoryLister` and one injected
  `FileLauncher` for its whole lifetime. It keeps the raw listing and
  re-derives the visible (hidden-filtered, sorted) snapshot when
  presentation settings change, without re-reading the directory. It
  publishes complete navigation/listing/launch-error state through Qt
  properties and never shows a dialog, chooses a selection, or retries on
  its own. Entry snapshots carry preformatted `sizeText`/`kindText`/
  `modifiedText` so QML delegates stay presentation-only.
- `BookmarksStore` (`model/bookmarks_store.h`) owns the versioned, bounded,
  symlink-refusing bookmark file beneath `$XDG_STATE_HOME` with atomic
  same-directory replacement (ADR-0090). `PlacesController` owns the fixed
  places list, bookmark add/remove/dedup/cap policy, and typed store-error
  publication; it never navigates or lists directories.
- `MutationBackend` (`mutation/mutation_backend.h`) is the synchronous,
  worker-thread operation seam. `LocalMutationBackend` owns create, rename,
  copy, and move policy; `HomeTrash` owns only Trash/restore/empty behavior;
  `DeviceResolver` isolates device identity for deterministic cross-device
  refusal tests.
- `MutationController` owns backend lifetime, worker scheduling, progress,
  cancellation, typed presentation state, serialized batch execution, and
  one-level recovery. It never lists folders, shows dialogs, or acquires
  shell/service authority.
- `fileManagerActionCatalog()` contributes the closed mutation, view, edit,
  and navigation action set to AppShell. `ApplicationCoordinator` transports
  activation and close requests but never examines a path or decides whether
  an operation is recoverable.
- `composeFileManagerMenuExport()` is the application composition boundary. It
  lends the primary window, coordinator, and session-bus connection to the
  opt-in AppShell exporter; it contains no filesystem or shell authority.
- QML (`ui/Main.qml` and its `Toolbar`/`Breadcrumb`/`LocationBar`/
  `PlacesSidebar`/`EntrySelection`/`EntryList`/`EntryGrid`/`StatePane`
  collaborators) owns only presentation: layout, keyboard routing to the
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

## QST-1 theme and accessibility boundary

Confirmed Settings1 theme and color-scheme changes publish live QST generations
through [ADR-0080](../adr/0080-resolve-first-party-appearance-from-settings.md).
An explicit `--theme` locks the process theme; `--theme-directory` extends the
validated schema-v1 theme search, using the same
search order (explicit directory, then `XDG_DATA_DIRS`-discovered
`qindaqt/themes`, then the install-relative theme directory). The resolved
`ThemeSpec` is derived into one QST-1 generation and published into the QML
engine's `QindaQt.Tokens 1.0` singleton before the real root QML is created,
so every `QindaQt.Controls` binding observes a complete generation on its
first evaluation; see the `AGENT-CONTRACT` comment in `main.cpp` and
`token_facade.h` for the exact publish-before-construct ordering this
depends on. `--check-theme` resolves the selected theme, prints its
identifier and QST revision, and exits before constructing a window, matching
the Text Editor's packaging-proof diagnostic.

The folder list and grid, sort headers, breadcrumb buttons, location field,
places and bookmark rows, toolbar buttons, and every state card expose
accessible names/roles/descriptions through `Accessible.role`/`.name` on each
QML item and through `QindaQt.Controls`' own accessible contracts (`Button`,
`Label`, `StateCard`). List entries additionally state whether they are a
folder or a file in their accessible name so a screen reader user does not
have to rely on icon shape or color alone.

## Desktop integration and verification

The visual revision adds focused local-decoder and asynchronous-provider tests
for resource admission, corrupt input, cancellation, two-job concurrency,
generation fencing, cache identity rechecks and listing refresh. A test-only
`qindaqt_file_manager_visual_probe` instantiates production QML and real local
controllers, creates its own browsing fixture, and saves an offscreen capture.
It accepts source root, theme ID, output path, width and height; an optional
last argument enables reduced transparency. Output belongs under ignored build
or cache directories. This probe does not add a production screenshot API.


`org.qindaqt.FileManager.desktop` registers the ordinary Wayland application
for `inode/directory` with one `%u` local-folder argument. Multiple folder
arguments, and a positional argument that is not a folder, are both rejected
rather than silently opening the wrong location. Positional validation occurs
before theme discovery so the documented exit and diagnostic remain stable in
a minimal or deliberately sanitized package environment.

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
in a clean disposable prefix. That component intentionally carries its required Tokens
and Controls backing libraries, plugins, metadata, and Controls QML sources.
With ambient QML and library paths cleared, the gate rejects an executable
that embeds the build QML directory, validates every built-in theme through
`--check-theme`, and constructs the real File Manager QML root offscreen
through `--check-qml-root` before exiting deterministically. The executable's
QML import root and Tokens loader path are relative to its installed location,
so the same component remains usable after staging or relocation. Token import
resolution may complete asynchronously even for a local installed module; the
startup boundary waits at most five seconds and reports either the QML error or
an explicit timeout before it attempts singleton publication.

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
- **S3** — power features: drag-and-drop, in-app search and a
  preview pane, a properties dialog, an open-with chooser (requires widening
  ADR-0029's launch contract through a new ADR), optional permanent deletion,
  refinement beyond the shipped public Controls icon boundary (ADR-0111).
- **S4** — volumes: mount enumeration and per-volume Trash (supersedes the
  ADR-0064 deferral with its own ADR; coordinate polkit/udisks boundaries
  through the Program Manager thread first).
- **S5** — network locations (SMB/SFTP-style browsing) behind a new injected
  backend seam and ADR; the lister contract currently forbids network and
  portal locations.

## Bounded deferrals

- Sort column/direction, hidden visibility, and list/grid mode are
  session-local; persisting them is a Settings1 schema decision deferred per
  ADR-0090.
- Batch operations are not covered by undo or Restore Last (one-level,
  single-item recovery is unchanged from S1).
- Permanent deletion outside confirmed Empty Trash, per-volume Trash, mounts,
  search, additional preview formats, portal-mediated paths, drag-and-drop,
  open-with, and network locations remain explicit later outcomes (S3–S5).
- One-level undo/restore is process-local and deliberately not a durable
  recovery journal. Single-item copy has no undo; users can trash its
  destination in a separate confirmed action.
- The offscreen UI rows prove construction, stable action/object identity,
  fatal-warning cleanliness, and the fixture-local identity-carrying mutation
  path. Nested screenshots and whole-application
  assistive-technology qualification remain later release evidence.
