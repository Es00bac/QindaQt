# QindaQt Text Editor

`qindaqt-editor` is QindaQt's lightweight first-party local UTF-8 plain-text
editor. It is a normal desktop client, not a shared application framework.
Document policy, local persistence, Settings1 policy consumption, and Qt
Widgets presentation have separate owners under `src/apps/text_editor`.

[ADR-0022](../adr/0022-keep-text-documents-local-and-atomic.md) owns the
per-document persistence guarantees. S2 keeps those guarantees for every tab
and adds the bounded multi-document and paths-only restore decision in
[ADR-0065](../adr/0065-persist-text-editor-path-inventory.md). The window
participates in [QindaQt.AppShell 1.0](application-shell.md) as established by
[ADR-0027](../adr/0027-extract-a-narrow-first-party-application-shell.md).

## Multi-document experience

One window hosts at most 32 tabs. Each tab owns an independent
`DocumentController`, `DocumentState`, `DocumentStore`, file watcher, editor
widget, external-change banner, and Qt undo history. Switching tabs changes
only presentation and active commands; a tab never borrows another tab's path,
byte revision, dirty state, warning, selection, or history.

The CLI and desktop entry accept multiple local paths. For example,
`qindaqt-editor a.txt b.txt` opens both. Open, drag-and-drop, and CLI admission
canonicalize each path through the document boundary. A canonical path already
present in the window selects its existing tab instead of creating a duplicate.
Save As also refuses a destination owned by another tab. The first successful
open may replace the pristine launch-time untitled tab; subsequent opens add
tabs. Invalid, unreadable, non-UTF-8, or oversized targets fail without changing
any admitted document.

Tab labels use only sanitized presentation data derived from the base file
name. Controls and invalid UTF-16 are removed, format characters are ignored,
whitespace is collapsed, and the result is bounded to 128 UTF-16 units. The
full canonical path remains a tooltip, never an unbounded tab label. The native
tab bar exposes an accessible `Document tabs` name and description; tab text
provides each tab's accessible name and selected state.

Closing one dirty tab offers Save, Discard, or Cancel for that document.
Closing the window summarizes at most eight dirty names in at most 512
characters and offers Save All, Discard All, or Cancel. Save All applies the
same per-document Save/Save As and external-revision guarantees; the first
failure cancels closure and preserves all still-open state.

## AppShell participation

Every command is a persistent window-scoped `QAction`. Its object name is the
widget compatibility identity; its lowercase dotted ID is the independent
AppShell catalog identity. AppShell activation and visible menus trigger the
same local action.

| Widget action | AppShell identity | Shortcut | Meaning |
| --- | --- | --- | --- |
| `fileNewAction` | `file.new` | `Ctrl+N` | Add an untitled tab |
| `fileOpenAction` | `file.open` | `Ctrl+O` | Choose and open a local document |
| `fileCloseTabAction` | `file.close-tab` | `Ctrl+W` | Close the active tab with consent |
| `fileSaveAction` | `file.save` | `Ctrl+S` | Save against the active byte revision |
| `fileSaveAsAction` | `file.save-as` | `Ctrl+Shift+S` | Choose a distinct local target |
| `fileQuitAction` | `file.quit` | `Ctrl+Q` | Close the window with bounded consent |
| `editUndoAction`, `editRedoAction` | `edit.undo`, `edit.redo` | Qt standard | Traverse only the active tab's history |
| `editCutAction`, `editCopyAction`, `editPasteAction` | `edit.cut`, `edit.copy`, `edit.paste` | Qt standard | Edit the active selection |
| `editSelectAllAction` | `edit.select-all` | `Ctrl+A` | Select the active document |
| `editFindAction`, `editReplaceAction` | `edit.find`, `edit.replace` | `Ctrl+F`, `Ctrl+H` | Open the in-window bar |
| `editFindNextAction`, `editFindPreviousAction` | `edit.find-next`, `edit.find-previous` | `F3`, `Shift+F3` | Traverse matches with wrap |
| `editFindCloseAction` | `edit.find-close` | `Escape` | Close the bar and restore editor focus |
| `tabNextAction`, `tabPreviousAction` | `tabs.next`, `tabs.previous` | `Ctrl+Tab`, `Ctrl+Shift+Tab` | Traverse tabs with wrap |
| `tabSelect1Action` … `tabSelect9Action` | `tabs.select-1` … `tabs.select-9` | `Ctrl+1` … `Ctrl+9` | Select a numbered tab when present |
| `restoreDocumentsAction` | `settings.restore-documents` | `Ctrl+Alt+R` | Toggle confirmed paths-only restore policy |

The catalog is published atomically across File, Edit, Tabs, and Settings
menus. Live enabled and checked state is projected back to AppShell. The window
routes close consent through AppShell's exact quit lineage. Open and Save As
use the injected `FileSelectionAdapter`; production uses the native chooser,
while missing test composition fails closed without touching a host chooser.

After the window is shown, the executable composes the first-party global-menu
export through the shared
`QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport` entry with its
coordinator, the window's platform `QWindow`, and its session-bus connection;
the composition, lifecycle, and fail-closed rules are owned by
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

When disabled, the editor removes its local restore inventory. When enabled,
it atomically writes `open-documents-v1.json` beneath
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
Explicit CLI paths suppress restore for that launch.

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
targets block Save and retain local text, exposing that tab's persistent,
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
- `DocumentCollection` alone owns controller admission, removal, and canonical
  path uniqueness for one window.
- `FindReplaceEngine` owns pure bounded search policy.
- `RestoreStateStore` owns only the injected state directory and paths-only
  JSON; `TextEditorRestorePolicy` owns only confirmed Settings1 policy.
- `EditorWindow` owns tabs, actions, focus, dialogs, accessibility, and command
  routing; it never imports shell/compositor internals or Settings1 transport.

The internal support headers are not installed or ABI-stable. The executable,
desktop ID, multi-file launch contract, documented shortcuts, widget action
names, and AppShell IDs are the compatibility surface.

## Theme, packaging, and verification

The Qt Widgets presentation derives its palette, fonts, focus ring, semantic
surfaces, and text colors from public QST-1 values. It imports no shell or
Controls internals and has no fallback brand palette. `qinda-dark` is the
default; `--theme` plus an optional local `--theme-directory` select a validated
schema-v1 theme. `--check-theme` verifies installed theme/QST identity and exits
before Settings1 or user-state composition. `--report-startup` reports only
after the real top-level window's first paint.

`org.qindaqt.TextEditor.desktop` registers `text/plain` with `%F`. The focused
selector is:

```sh
ctest --test-dir build/dev -R '^qindaqt\.editor-' --output-on-failure
```

The rows cover document invariants, UTF-8/BOM/size and atomic failures,
external-change conflict recovery, duplicate/colliding paths, independent tab
state and undo, bounded consent summaries and title sanitization, all action
catalog identities and keyboard traversal, find options/wrap/no-match and
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
qualification remain later outcomes. Dirty-save and destination-replacement
consent still use direct `QMessageBox` presentation. The desktop entry keeps
the platform `accessories-text-editor` icon until a branded asset lands.
