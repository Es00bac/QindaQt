# QindaQt File Manager

`qindaqt-file-manager` is QindaQt's first-party local-directory browser. S0
landed bounded listing, navigation, and regular-file launch. S1 adds local new
folder, rename, copy, same-filesystem move, home Trash, restore, and empty-Trash
operations with cooperative cancellation, progress, typed failure, and
one-level recovery. Mounts, search, previews, portals, per-volume Trash, and
network locations remain explicit later slices.

The durable local-launch choice is recorded in
[ADR-0029](../adr/0029-file-manager-bounded-local-launch.md); the S1 mutation
and Trash authority is recorded in
[ADR-0064](../adr/0064-confine-file-mutation-to-identity-checked-local-authority.md).

S1 composes `QindaQt.Tokens 1.0`, `QindaQt.Controls 1.0`, and the public
`QindaQt.AppShell 1.0` window/action/lifecycle boundary. File Manager retains
all navigation and filesystem policy; AppShell owns only the standard menu,
shortcut dispatch, focus reporting, and close-decision protocol (see
[Module boundaries](../architecture/module-boundaries.md)).

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
| `file.trash` | `Delete` | Confirm and move the selected item to recoverable home Trash |
| `file.restore-last` | `Ctrl+Shift+R` | Restore the most recently trashed item to its recorded path |
| `file.empty-trash` | `Ctrl+Shift+Delete` | Confirm and permanently empty home Trash |
| `edit.undo` | platform Undo | Undo the last recoverable create, rename, or move |
| `operation.cancel` | `Ctrl+Escape` | Request cancellation of the running operation |

The toolbar's `newFolderButton`, list's `entryListView`, dialogs, and bounded
progress/failure/result cards have stable object names for the offscreen UI
contract. Dialog fields and context actions expose accessible text. The list
supports the Menu key and `Shift+F10`, so mutation does not depend on a
pointer-only context menu.

`MutationController` is GUI-thread confined and owns one injected
`MutationBackend`, one worker thread, and at most one in-flight operation. It
publishes bounded progress and terminal state. Cancellation is cooperative;
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
S1 intentionally does not infer mount roots or create `.Trash`/`.Trash-$uid`
directories. Empty Trash removes entries below `files/` and `info/` without
following links and retains those two directories.

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
- `MutationBackend` (`mutation/mutation_backend.h`) is the synchronous,
  worker-thread operation seam. `LocalMutationBackend` owns create, rename,
  copy, and move policy; `HomeTrash` owns only Trash/restore/empty behavior;
  `DeviceResolver` isolates device identity for deterministic cross-device
  refusal tests.
- `MutationController` owns backend lifetime, worker scheduling, progress,
  cancellation, typed presentation state, and one-level recovery. It never
  lists folders, shows dialogs, or acquires shell/service authority.
- `fileManagerActionCatalog()` contributes the closed mutation action set to
  AppShell. `ApplicationCoordinator` transports activation and close requests
  but never examines a path or decides whether an operation is recoverable.
- QML (`ui/Main.qml` and its `Toolbar`/`Breadcrumb`/`EntryList`/`StatePane`
  collaborators) owns only presentation: layout, keyboard routing to the
  controller's invokable methods, accessible names/roles, and the
  presentation-owned `ListView` selection index. It never lists a directory or
  launches a file itself.

All expected errors cross the lister/launcher/mutation boundaries as typed
values plus bounded human-readable diagnostics. There is no D-Bus authority,
shell-private dependency, global worker pool, or exception-based failure
channel. The one mutation worker and its backend are private implementation
details with constructor-visible ownership.

The `model/**` C++ headers and build target are private implementation
surfaces and are not installed or ABI-stable. The executable name, desktop ID,
folder-launch-argument contract, and documented action object names/shortcuts
form the compatibility surface.

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
contract against injected fakes, desktop metadata, and CLI arity/argument
validation. The package row stages only the `FileManager` component in a clean
disposable prefix. That component intentionally carries its required Tokens
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
boundary-policy rows. Fixture trees exercise Unicode/control-character names,
overlong rejection, permissions, collision, before/during-operation vanishing,
identity change, nested and root symlink poison, cancellation cleanup, Trash
round trips, an in-flight nested-directory swap, unique names, restore
collision, orphan-payload allocation, vanished restore parents, empty Trash,
an injected cross-device refusal, and a racing destination writer. One
offscreen row constructs the production QML root; a second drives the real
AppShell actions through production dialogs for rename, copy, move, Trash, and
restore against a disposable tree. Both run under `QT_FATAL_WARNINGS=1` with
host display and session-bus variables removed. The
boundary checker rejects dependencies on shell/services, D-Bus, KWin,
LayerShell, desktop launch, or QML from the mutation module and proves itself
against planted poison. All Trash roots live below disposable fixture/build
directories; no row reads or mutates the user's home Trash.

## Bounded S1 deferrals

- There is no editable address bar; the breadcrumb's clickable segments are
  S0's complete direct-path-entry surface. A typed/pasted path field is a
  later presentation slice.
- There is no multi-select, drag-and-drop, file-size/date column formatting,
  icon-theme integration, hidden-file toggle, or general editable address bar.
- Permanent deletion outside confirmed Empty Trash, per-volume Trash, mounts,
  search, previews/thumbnails, portal-mediated paths, and network locations
  remain explicit later outcomes.
- One-level undo/restore is process-local and deliberately not a durable
  recovery journal. Copy has no undo; users can trash its destination in a
  separate confirmed action.
- The offscreen UI rows prove construction, stable action/object identity,
  fatal-warning cleanliness, and the fixture-local identity-carrying mutation
  path. Nested screenshots and whole-application
  assistive-technology qualification remain later release evidence.
