# QindaQt Text Editor

`qindaqt-editor` is QindaQt's lightweight first-party local UTF-8 plain-text
editor. It is a normal desktop client, not a shared application framework.
Document policy, local persistence, Settings1 policy consumption, and Qt
Widgets presentation have separate owners under `src/apps/text_editor`.

[ADR-0022](../adr/0022-keep-text-documents-local-and-atomic.md) owns the
per-document persistence guarantees. The paths-only restore decision in
[ADR-0065](../adr/0065-persist-text-editor-path-inventory.md) remains compatible;
[ADR-0099](../adr/0099-own-editor-documents-in-ordinary-windows.md) supersedes its
single-window hosting choice. Each window
participates in [QindaQt.AppShell 1.0](application-shell.md) as established by
[ADR-0027](../adr/0027-extract-a-narrow-first-party-application-shell.md).

## Everyday editing

Line numbers and a highlighted current line make long files easier to follow.
Use **Edit → Go to Line** (`Ctrl+G`) to jump to a line from an error message.
The status bar shows your line, column, document language, and whether you have
unsaved changes. File names select syntax highlighting automatically, including
Python, shell scripts, JSON, C++, and Markdown. Unknown file types stay plain text.

Tab inserts spaces to the next four-column stop. Select several lines and press
Tab to indent them together; Shift+Tab moves them back. Enter carries the current
line's leading whitespace onto the next line. Each indentation command is one
undo step. Find and Replace remain available with `Ctrl+F` and `Ctrl+H`.

The **View** menu controls word wrapping and text size. `Ctrl++` and `Ctrl+-`
zoom; `Ctrl+0` restores the theme's size. Wrapping and zoom belong to each open
window and do not change the file or survive closing it.

`DocumentEditor` owns these presentation and input behaviors. Its confined
KF6 SyntaxHighlighting dependency is recorded in
[ADR-0095](../adr/0095-use-ksyntaxhighlighting-for-editor-presentation.md).
Highlighting never rewrites the document, changes its saved encoding, or creates
an undo step. Document policy and disk access remain with their existing owners.

## Documents are ordinary windows

Every document has one ordinary top-level window. The desktop's containers own
window grouping, tabs and splits; the editor has no tab strip, Tabs menu or
private document-switching shortcuts. **New** creates a new untitled window.
Closing a window offers Save, Discard or Cancel for that document only. Both
`Ctrl+W` and `Ctrl+Q` close the current window; other documents remain open.

`EditorApplication` owns a bounded inventory of at most 32 windows on the GUI
thread. Each window owns its independent `DocumentController`, `DocumentState`,
`DocumentStore`, watcher, view, undo history, search bar and AppShell coordinator.
No document window owns another window. Closing the first window does not
retire any other window or its menu export.

The CLI and desktop entry accept multiple local paths. For example,
`qindaqt-editor a.txt b.txt` opens two windows. Open, drag-and-drop and CLI
admission canonicalize each path through the document boundary. A canonical
path already open in this process activates its window. Save As refuses a
path owned by another window. A pristine untitled window may accept a document;
a dirty untitled window keeps its contents and opens the new file separately.
Invalid, unreadable, non-UTF-8 or oversized targets fail without changing
successfully admitted documents. The CLI reports partial failures and keeps
successful document windows available.

Window titles use sanitized base filenames, bounded to 128 UTF-16 units.
Controls and invalid UTF-16 are removed, format characters ignored, and
whitespace collapsed. The canonical path remains document state and window
metadata; it is never an unbounded display title.

## AppShell participation

Every command is a persistent window-scoped `QAction`. Its object name is the
widget compatibility identity; its lowercase dotted ID is the independent
AppShell catalog identity. AppShell activation and visible menus trigger the
same local action.

| Widget action | AppShell identity | Shortcut | Meaning |
| --- | --- | --- | --- |
| `fileNewAction` | `file.new` | `Ctrl+N` | Create an untitled window |
| `fileOpenAction` | `file.open` | `Ctrl+O` | Choose and open a local document |
| `fileCloseWindowAction` | `file.close-window` | `Ctrl+W` | Close this window with document consent |
| `fileSaveAction` | `file.save` | `Ctrl+S` | Save against the active byte revision |
| `fileSaveAsAction` | `file.save-as` | `Ctrl+Shift+S` | Choose a distinct local target |
| `fileQuitAction` | `file.quit` | `Ctrl+Q` | Close the window with bounded consent |
| `editUndoAction`, `editRedoAction` | `edit.undo`, `edit.redo` | Qt standard | Traverse only the window's history |
| `editCutAction`, `editCopyAction`, `editPasteAction` | `edit.cut`, `edit.copy`, `edit.paste` | Qt standard | Edit the active selection |
| `editSelectAllAction` | `edit.select-all` | `Ctrl+A` | Select the active document |
| `editFindAction`, `editReplaceAction` | `edit.find`, `edit.replace` | `Ctrl+F`, `Ctrl+H` | Open the in-window bar |
| `editFindNextAction`, `editFindPreviousAction` | `edit.find-next`, `edit.find-previous` | `F3`, `Shift+F3` | Traverse matches with wrap |
| `editFindCloseAction` | `edit.find-close` | `Escape` | Close the bar and restore editor focus |
| `editGoToLineAction` | `edit.go-to-line` | `Ctrl+G` | Jump to a line |
| `editIndentAction`, `editUnindentAction` | `edit.indent`, `edit.unindent` | `Ctrl+]`, `Ctrl+[` | Indent or unindent selected lines |
| `viewWordWrapAction` | `view.word-wrap` | `Ctrl+Alt+W` | Toggle wrapping for this window |
| `viewZoomInAction`, `viewZoomOutAction`, `viewZoomResetAction` | `view.zoom-in`, `view.zoom-out`, `view.zoom-reset` | `Ctrl++`, `Ctrl+-`, `Ctrl+0` | Change or reset this window’s text size |
| `restoreDocumentsAction` | `settings.restore-documents` | `Ctrl+Alt+R` | Toggle confirmed paths-only restore policy |

The catalog is published atomically across File, Edit, View, and Settings
menus. Live enabled and checked state is projected back to AppShell. The window
routes close consent through AppShell's exact quit lineage. Open and Save As
use the injected `FileSelectionAdapter`; production uses the native chooser,
while missing test composition fails closed without touching a host chooser.
Visible catalog labels preserve their UTF-8 punctuation, including the
ellipsis in `Open…`, `Find…`, and `Save As…`, through the AppShell snapshot.

After each window is shown, the executable composes the first-party global-menu
export through the shared
`QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport` entry with its
coordinator, the window's platform `QWindow`, and its session-bus connection;
the export is retained by its window and destroyed before its coordinator.
The composition, lifecycle, and fail-closed rules are owned by
[the global-menu page](../shell/global-menu.md). A missing session bus or
registrar leaves the export disabled/waiting, and the local `QMenuBar` stays
visible and authoritative.

## Find and replace

Find and replace is an in-window bar, not a modal dialog. It offers literal or
regular-expression search, case sensitivity, whole-word matching, next and
previous traversal with wrap, Replace, and Replace All. A textual status gives
the current match and total (including whether traversal wrapped), or says
`No matches`; changes are also announced politely through accessibility APIs.
Meaning never depends on color.

Patterns are bounded to 256 UTF-16 units and results to 10,000 matches.
User regexes pass through `QRegularExpression` only after a deliberately linear
admission policy: literals, escapes, character classes, anchors, and dot are
accepted; quantifiers, grouping, alternation, lookaround, and backreferences
are rejected before document scanning. This narrower-than-PCRE contract is the
complexity bound that keeps hostile patterns off an unbounded GUI-thread path.
Invalid admitted expressions and excessive matches fail with bounded status.

Replace changes one match. Replace All calculates the bounded match inventory
before mutation, applies replacements from the end, and wraps the complete
operation in one `QTextCursor` edit block, so one Undo restores the document.

## Restore policy and state

`services.textEditorRestoreDocuments` is a schema-v1/v2 Boolean with default
`false`. `TextEditorRestorePolicy` scopes one existing Settings1 client to that
key. Only a complete Boolean snapshot establishes truth. Missing ownership,
malformed snapshots, and transport loss immediately fall back to disabled.
Writes are serialized against the confirmed epoch/revision: conflicts require
an explicit retry, a successful commit is not accepted until the mandatory
fresh snapshot confirms it, and an uncertain write is never replayed.

`EditorApplication` is the sole process-local inventory writer. Before the
first Settings1 baseline arrives, the saved inventory is retained
without being loaded. Once policy truth has been received, disabled policy or
owner loss removes the local restore inventory. When enabled, it atomically
writes `open-documents-v1.json` beneath
`$XDG_STATE_HOME/qindaqt/text-editor` using `QSaveFile` with direct-write
fallback disabled and owner-only file permissions. Directory traversal opens
each absolute-path component with Linux `openat`/`O_NOFOLLOW` semantics and
keeps the final directory descriptor through atomic commit; any symlinked
ancestor or unsafe final entry refuses load, store, and clear rather than
escaping the selected state path. The exact schema is:

```json
{"version":1,"paths":["/absolute/a.txt"],"activeIndex":0}
```

The file is capped at 64 KiB, 32 unique absolute paths, and 4,096 UTF-16 units
per path. Its root keys and value types are exact; the active index must name a
listed path. A symlink, non-regular file, malformed JSON, unknown key,
duplicate/relative path, non-integral index, or limit violation rejects the
inventory wholesale. On startup each valid listed path still passes through
ordinary document admission. Missing, unreadable, invalid UTF-8, and oversized
documents are silently skipped, with one bounded accessible count notice.
Explicit CLI paths suppress restore for that launch, including an already-confirmed
policy present before window creation. Restored paths reopen as ordinary windows;
`activeIndex` selects the preferred active window. Closing one of several windows
removes its path from the inventory. The final close retains the last window’s
path for the next launch. Unchanged inventories are not rewritten on every edit.

The inventory contains no text, dirty flag, selection, history, byte revision,
or other content-bearing value. Therefore restore can reopen only current disk
content and can never restore dirty content. Autosave, content journals, crash
recovery, and revision history remain excluded.

## Per-document persistence and failures

The editor accepts local regular files containing valid UTF-8 up to 32 MiB. A
UTF-8 BOM is removed for editing and restored on save. CRLF, LF, and legacy CR
are normalized internally; the dominant opened convention is restored (CRLF
wins a mixed tie), and missing final newlines remain missing. Malformed UTF-16
is rejected before encoding. Existing symbolic links resolve once to their
canonical regular-file targets; dangling links are not valid destinations.

Every opened document retains the SHA-256 and byte count of the exact bytes
read. Normal Save rechecks that revision. Changed, missing, or unreadable
targets block Save and retain local text, exposing that window’s persistent,
textual, accessible banner. Reload and Save As are explicit recovery paths.
Save As refuses an existing destination until separate replacement consent.
Successful writes use `QSaveFile` atomically with direct-write fallback
disabled. This is optimistic concurrency with an acknowledged compare-to-
rename race, not a lock or collaborative-editing protocol.

Expected errors cross boundaries as typed values plus bounded diagnostics:

- `DocumentState` is a thread-neutral in-memory value with no I/O.
- `DocumentStore` is injected synchronous persistence;
  `LocalDocumentStore` owns bounded filesystem and encoding work.
- `DocumentController` is GUI-thread confined and owns one store and watcher.
- `EditorApplication` owns window admission, removal, process-local canonical
  uniqueness and the sole paths-only restore inventory.
- `FindReplaceEngine` owns pure bounded search policy.
- `RestoreStateStore` owns only the injected state directory and paths-only
  JSON; `TextEditorRestorePolicy` owns only confirmed Settings1 policy.
- `EditorWindow` owns one controller/view, actions, focus, dialogs, accessibility, and command
  routing; it never imports shell/compositor internals or Settings1 transport.

The internal support headers are not installed or ABI-stable. The executable,
desktop ID, multi-file launch contract, documented shortcuts, widget action
names, and AppShell IDs are the compatibility surface.

## Theme, packaging, and verification

The compact icon toolbar provides New, Open, Save, Save As and Find with
keyboard focus, tooltips and accessible names. A softly layered chrome gradient
frames the opaque document canvas and roomy line-number gutter. Search uses
icon buttons with accessible names and compact options; errors appear only
when recovery is needed. The shared icon catalog supplies branded assets.

The Qt Widgets presentation derives its palette, fonts, focus ring, semantic
surfaces, and text colors from public QST-1 values. It imports no shell or
Controls internals and has no fallback brand palette. `qinda-dark` is the
fallback. Without an explicit `--theme`, confirmed Settings1 theme and color
scheme changes are resolved by [ADR-0080](../adr/0080-resolve-first-party-appearance-from-settings.md)
and applied live to every window and document view. Confirmed interface and
monospace font families, point size, text scaling and accessibility preferences
reach the same QST adapter. High contrast and reduced transparency preserve
opaque reading surfaces and explicit focus outlines. High contrast retains
syntax weight and emphasis while using semantic text colors. Transparent syntax
foregrounds are rejected so Markdown headings never disappear. `--theme` locks a validated
schema-v1 theme; `--theme-directory` extends discovery. `--check-theme` verifies installed theme/QST identity and exits
before Settings1 or user-state composition. `--report-startup` reports only
after the real top-level window's first paint.

`org.qindaqt.TextEditor.desktop` registers `text/plain` with `%F`. The focused
selector is:

```sh
ctest --test-dir build/dev -R '^qindaqt\.editor-' --output-on-failure
```

The rows cover document invariants, UTF-8/BOM/size and atomic failures,
external-change conflict recovery, duplicate/colliding paths, independent window
state and undo, window-local close cancellation, multi-file drops, title
sanitization, and action-catalog identities, find options/wrap/no-match and
hostile regex rejection, single-step Replace All, restore-state schema and
final/ancestor-symlink and oversize rejection, Settings1
baseline/conflict/uncertainty, multi-path and hostile CLI admission, desktop
metadata, source-boundary poison, and a clean installed-prefix offscreen
launch. The global-menu export slice adds real-process private-bus rows under
the selector documented in [the global-menu page](../shell/global-menu.md):
exact-identity export with one shell activation and provider-exit clearing,
mismatched PID/window variants, registrar-absent late binding, and
hostile-registrar fail-closed rebinding. Every row that launches offscreen Qt
registers
`QT_FATAL_WARNINGS=1`; a registry-policy row checks the three script-driven
and package cases whose names do not express that requirement. All rows receive
build-root HOME, XDG state/data, and TMPDIR values and disconnected display/bus
environments.

The installed performance probe retains the S1 ceilings: first painted frame
at most 400 ms and median PSS at most 64 MiB across five settled samples.
These are offscreen process checks, not nested-session, hardware, real portal,
global-menu transport, or assistive-technology qualification.

## Bounded deferrals

Portals beyond the injected file-selection seam, printing, extensions, rich
text, remote URLs, content journaling/autosave, collaborative locking or merge,
a nested display screenshot matrix, and whole-application assistive-technology
qualification remain later outcomes. Dirty-save and destination-replacement consent use a window-owned native
`DocumentDialogs` adapter backed by `QMessageBox`; focused tests inject explicit
decisions without a host popup. The desktop entry selects
the branded `org.qindaqt.TextEditor` icon through the public application icon catalog.

### Editing-tools acceptance

`qindaqt.editor-editing-tools-offscreen` exercises multi-line indentation and
undo, Enter/Tab input, syntax detection without content or dirty-state changes,
line navigation, gutter sizing, and theme-preserving zoom. The real window row
also verifies that wrapping and zoom follow their document across separate windows and live appearance changes.


### Window and visual acceptance

`qindaqt.editor-application-offscreen` proves canonical window admission,
independent per-window actions, close Save/Discard/Cancel, safe Save As collisions,
local multi-file drops, first-window lifetime and the 32-window bound.
`qindaqt.editor-visual-offscreen` captures actual wide and compact application
windows, with search open and closed, under light, dark and high-contrast themes.
Captures remain in the ignored build tree. It checks window geometry, document
content and visible heading formats; screenshots require visual review and do
not claim compositor/nested qualification.
