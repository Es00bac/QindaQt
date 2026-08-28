# QindaQt File Manager

`qindaqt-file-manager` is QindaQt's first-party local-directory browser. S0 is
one bounded local-navigation vertical slice: list a folder, move through
history/breadcrumb/up, and open a regular file through a bounded launch
intent. Destructive file operations (copy/move/delete/rename), mounts, trash,
search, thumbnails, portals, and network locations are explicit later slices,
not silently implied by this page.

The durable local-launch and app-composition choices are recorded in
[ADR-0029](../adr/0029-file-manager-bounded-local-launch.md).

S0 uses `QindaQt.Tokens 1.0` and `QindaQt.Controls 1.0` QML directly. It is
the first first-party application to consume the public Controls boundary in
production rather than only in Controls' own test harness. The independently
integrated `QindaQt.AppShell 1.0` boundary is available, but S0 deliberately
keeps its already-reviewed bespoke composition; migrating this app is a later
File Manager vertical slice, not an unreviewed addition to local navigation
(see [Module boundaries](../architecture/module-boundaries.md)).

## S0 user experience

One window browses one local folder tree at a time, starting at the user's
home folder unless a valid local folder is given on the command line. The
toolbar provides Back, Forward, Up, and Refresh. A breadcrumb bar below it
shows every path segment from the filesystem root to the current folder as a
clickable button; clicking any segment navigates straight there. The main
pane lists the current folder's immediate children, directories first, then
files, both case-insensitively ordered by name. When the bounded lister has
to stop early, a muted notice below the list states how many entries are
shown instead of silently hiding the remainder.

| Action identity | Shortcut | Meaning |
| --- | --- | --- |
| `navigateBackButton` | `Alt+Left` | Return to the previous folder in history |
| `navigateForwardButton` | `Alt+Right` | Return to the folder undone by Back |
| `navigateUpButton` | `Alt+Up`, `Backspace` (when the list has focus) | Go to the parent folder |
| `refreshButton` | `F5`, `Ctrl+R` | Re-read the current folder |
| `entryListView` | `Return`/`Enter` | Open the selected entry |

Opening a directory entry navigates into it. Opening a file entry requests a
bounded local launch (see below); the list selection and current folder never
change just because a launch failed. A folder that cannot be listed (missing,
not a folder, permission denied, or an unclassified read failure) or that
lists cleanly but has no children presents one accessible
`QindaQt.Controls` `StateCard` instead of an empty or frozen-looking list, so
every non-Ready state is visibly distinct and never silently indistinguishable
from "still loading."

Selection is deterministic: whenever the current listing is rebuilt, the list
restores the previously selected entry by name when that name still exists in
the new listing, and otherwise selects the first entry (or nothing, if empty).
A navigation into a different folder therefore usually lands on the first
entry because the old selection's name rarely exists there, while a refresh
of the same folder keeps the user's place. `NavigationController::indexOfName()`
is the pure, testable seam presentation uses for that restoration.

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
  `FileLauncher` for its whole lifetime. It publishes complete navigation/
  listing/launch-error state through Qt properties and never shows a dialog,
  chooses a selection, or retries on its own.
- QML (`ui/Main.qml` and its `Toolbar`/`Breadcrumb`/`EntryList`/`StatePane`
  collaborators) owns only presentation: layout, keyboard routing to the
  controller's invokable methods, accessible names/roles, and the
  presentation-owned `ListView` selection index. It never lists a directory or
  launches a file itself.

All expected errors cross the `DirectoryLister`/`FileLauncher` boundary as
typed values plus a bounded, human-readable diagnostic. There is no background
worker, D-Bus authority, shell-private dependency, or exception-based failure
channel in S0.

The `model/**` C++ headers and build target are private implementation
surfaces and are not installed or ABI-stable. The executable name, desktop ID,
folder-launch-argument contract, and documented action object names/shortcuts
form the S0 compatibility surface.

## QST-1 theme and accessibility boundary

`--theme` (default `qinda-dark`) and an optional `--theme-directory` select a
validated schema-v1 theme exactly as the Text Editor does, using the same
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

The folder list, breadcrumb buttons, toolbar buttons, and every state card
expose accessible names/roles/descriptions through `Accessible.role`/`.name`
on each QML item and through `QindaQt.Controls`' own accessible contracts
(`Button`, `Label`, `StateCard`). List entries additionally state whether they
are a folder or a file in their accessible name so a screen reader user does
not have to rely on icon shape or color alone.

## Desktop integration and verification

`org.qindaqt.FileManager.desktop` registers the ordinary Wayland application
for `inode/directory` with one `%u` local-folder argument. Multiple folder
arguments, and a positional argument that is not a folder, are both rejected
rather than silently opening the wrong location.

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
contract against injected fakes, desktop metadata, and CLI arity/argument
validation. The package row stages only the `FileManager` component in a clean
disposable prefix. That component intentionally carries its required Tokens
and Controls backing libraries, plugins, metadata, and Controls QML sources.
With ambient QML and library paths cleared, the gate rejects an executable
that embeds the build QML directory, validates every built-in theme through
`--check-theme`, and constructs the real File Manager QML root offscreen
through `--check-qml-root` before exiting deterministically. The executable's
QML import root and Tokens loader path are relative to its installed location,
so the same component remains usable after staging or relocation.

## Bounded S0 deferrals

- There is no editable address bar; the breadcrumb's clickable segments are
  S0's complete direct-path-entry surface. A typed/pasted path field is a
  later presentation slice.
- There is no multi-select, rename, drag-and-drop, context menu, file-size/
  date column formatting, icon theme integration, or hidden-file toggle. List
  entries show only a name and a muted style for dotfiles.
- Copy, move, delete, rename, new-folder, mounts, trash, search, thumbnails,
  and any portal-mediated or network location remain explicit later outcomes.
- `QindaQt.AppShell 1.0` is not consumed in S0. Its public boundary is already
  accepted and integrated; adopting its lifecycle/action/window seams is an
  explicit later File Manager migration with its own interaction and regression
  evidence rather than a hidden expansion of this local-navigation slice.
