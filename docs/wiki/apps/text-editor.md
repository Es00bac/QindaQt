# QindaQt Text Editor

`qindaqt-editor` is QindaQt's lightweight first-party local plain-text editor.
S1 is intentionally one complete ordinary desktop-client outcome, not a shared
application framework: document policy, local filesystem persistence, and Qt
Widgets presentation have separate owners inside `src/apps/text_editor`.

The durable persistence and menu choices are recorded in
[ADR-0022](../adr/0022-keep-text-documents-local-and-atomic.md). The editor
also participates in the shared
[QindaQt.AppShell 1.0](application-shell.md) action/lifecycle boundary
established by
[ADR-0027](../adr/0027-extract-a-narrow-first-party-application-shell.md); see
[AppShell participation](#appshell-participation) below.

S1 uses Qt Widgets because `QPlainTextEdit`, `QMainWindow`, standard actions,
native dialogs, and accessibility adapters supply this narrow editor surface
with less resident presentation machinery. This follows the project rule that
Widgets are appropriate where measurably simpler and lighter. It is not a
desktop-wide framework choice and does not constrain later QML/Controls-based
Settings, file-manager, or shell experiences.

## S1 user experience

One editor process owns one document. The File menu provides New, Open, Save,
Save As, and Quit. The Edit menu provides Undo, Redo, Cut, Copy, Paste, and
Select All. Every command is a persistent top-level `QAction` with a stable
object name and the matching Qt standard shortcut. The menu bar is an ordinary
`QMainWindow` menu model so a later shell global-menu adapter can export it
without editor-private commands or duplicated action policy.
The command set may grow additively, but published object names and standard-
shortcut meanings remain stable within a release.

| Action identity | Linux default | Meaning |
| --- | --- | --- |
| `fileNewAction` | `Ctrl+N` | Start an untitled document after dirty consent |
| `fileOpenAction` | `Ctrl+O` | Open one local document after dirty consent |
| `fileSaveAction` | `Ctrl+S` | Save only when the external byte revision still matches |
| `fileSaveAsAction` | `Ctrl+Shift+S` | Choose a new local target with explicit replacement consent |
| `fileQuitAction` | `Ctrl+Q` | Close after dirty consent |
| `editUndoAction`, `editRedoAction` | Qt standard shortcuts | Traverse the widget document history |
| `editCutAction`, `editCopyAction`, `editPasteAction` | Qt standard shortcuts | Edit through the current selection/clipboard |
| `editSelectAllAction` | `Ctrl+A` | Select the complete document |

The editor opens local regular files containing valid UTF-8 up to 32 MiB. A
UTF-8 byte-order mark is removed for editing and restored on subsequent saves.
CRLF, LF, and legacy CR are normalized to the editor's internal LF paragraphs;
the dominant opened convention is restored on save (CRLF wins a mixed-input
tie). A missing final newline remains missing. Invalid UTF-8, directories,
missing open targets, and oversized files fail without replacing the current
buffer. Open and Save As dialogs are extension-neutral because the accepted
content contract is UTF-8 plain text rather than a filename suffix.
Malformed in-memory UTF-16 is rejected before encoding so a save can never
silently substitute replacement characters.

An existing symbolic-link path is resolved once to its canonical regular-file
target before the document is adopted, so an atomic save does not replace the
link itself. A dangling link is not a valid Save As destination.

Dirty state compares the current text exactly with the last successfully opened
or saved text. The window forwards Qt's UTF-16 edit deltas into the document
value, so ordinary typing does not copy and compare the complete buffer on each
keystroke; equal-length edits still receive an exact baseline comparison. New,
Open, Quit, and external Reload require Save, Discard, or Cancel consent when
dirty. The window's standard modified marker and Save action reflect that state.

## AppShell participation

`EditorWindow` owns one `EditorAppShellBridge`
(`src/apps/text_editor/app_shell/`), which owns the window's single
`QindaQt::AppShell::ApplicationCoordinator` for the primary-window lifetime.
The bridge never decides consent or command execution itself; `EditorWindow`
keeps that authority, matching the ownership boundary in
[QindaQt.AppShell 1.0](application-shell.md).

- **Action and menu export.** `editorActionCatalog()`
  (`app_shell/editor_action_catalog.h`) builds one atomic `ActionSpec`
  replacement for the documented File/Edit commands, published once during
  construction. Each AppShell action uses a lowercase dotted ID (`file.new`,
  `file.open`, `file.save`, `file.save-as`, `file.quit`, `edit.undo`,
  `edit.redo`, `edit.cut`, `edit.copy`, `edit.paste`, `edit.select-all`) —
  a separate identifier space from the stable QAction object names in the
  table above. `EditorWindow` keeps the published `enabled` projection in
  sync with the live QAction tree through `QAction::enabledChanged`, and
  routes `ApplicationCoordinator::actionRequested` back to the same local
  `QAction::trigger()` used by the visible menu, so a future global-menu
  consumer runs the identical local command path.
- **Close consent.** `closeEvent()` calls `requestQuit()` instead of running
  dirty consent directly. The coordinator's `quitDecisionRequested` handler
  runs the same `confirmDiscardOrSave()` dirty-consent policy and resolves
  the request; the window only accepts the close once `quitApproved` fires.
  Settings and session hooks stay `IntegrationState::NotRequired`: S1 has
  neither integration.
- **File selection.** Open and Save As no longer call `QFileDialog`
  directly. They issue an AppShell `requestOpenFile`/`requestSaveFile`
  portal request and wait for the injected `FileSelectionAdapter`
  (`app_shell/file_selection_adapter.h`) to resolve it.
  `EditorWindow` constructs `NativeFileSelectionAdapter` by default for the
  production application; it shows the same modal `QFileDialog` as before.
  A caller may inject another adapter, while `EditorAppShellBridge` itself
  substitutes `FailClosedFileSelectionAdapter` only when constructed with a
  null adapter. That bridge-level fallback denies every request without
  touching any host chooser, so a missing or misconfigured collaborator can
  never silently invent a path. A Save As suggests only the destination's
  base file name, not a full initial directory, matching the portal's
  sandbox-compatible contract.

The focused selector is:

```sh
ctest --test-dir build/dev -R '^qindaqt\.editor-app-shell-' --output-on-failure
```

It covers the published catalog against the real `ActionRegistry`
validation, the atomic initial menu snapshot and its synced enabled
projection, dirty-state projection, AppShell-activated commands running the
identical local trigger, close consent routed through `quitApproved`, the
fail-closed bridge fallback, typed Open/Save As cancellation with subsequent
request recovery, stale-reply fencing followed by exact-ID recovery, and a
full open resolved by an injected adapter. A dedicated source-policy row scans
the complete editor bridge/window seam, confines `QFileDialog` to the native
adapter, rejects private platform/service dependencies, and proves the matcher
with a generated poisoned fixture. These rows never open a real file chooser
or consent dialog; the existing
`qindaqt.editor-` regressions above remain the coverage for local dirty
consent and the file dialogs themselves. Real portal/global-menu adapters,
Settings/session hook composition, and a global-menu export transport remain
later outcomes per ADR-0027.

## External changes and atomic saves

An opened document retains the SHA-256 and byte count of the exact bytes read.
The editor watches both the file and its containing directory, then rechecks
the complete bounded byte revision after a short event debounce. A content
change, removal, or unreadable state exposes a persistent accessible banner;
it never replaces local text automatically. Changed content uses QST-1 warning
colors and a textual `Warning:` prefix. Missing or unreadable targets use QST-1
danger colors and an `Error:` prefix, and each state has a distinct status-bar
message so severity and meaning never depend on color alone.

Normal Save rechecks that exact baseline before opening the save transaction.
If the file changed, disappeared, or became unreadable, Save fails and retains
the user's buffer. Reload and Save As are the explicit recovery paths. A new
Save As refuses an existing destination until the presentation obtains a
separate replace confirmation. This includes selecting the current document
path after an external replacement: the external bytes remain untouched unless
the user explicitly confirms replacing that destination.

Successful writes use `QSaveFile` with direct-write fallback disabled. The
temporary file must commit through an atomic replacement supported by the
destination filesystem; otherwise the original remains and the operation
fails. This is an optimistic local-file contract, not a lock protocol: another
writer can still win the bounded interval between the revision check and the
atomic rename. Cross-process locks, collaborative editing, remote URLs,
autosave/recovery journals, and revision history remain later outcomes.

## Ownership, lifetime, and failures

- `DocumentState` owns only the in-memory path, text, saved-text baseline,
  encoding marker, byte revision, dirty state, and external-state projection.
  It performs no I/O and is a thread-neutral value used on one caller thread.
- `DocumentStore` is an injected synchronous boundary. `LocalDocumentStore`
  performs bounded local reads, UTF-8 conversion, digesting, optimistic compare,
  and atomic persistence. It never presents UI or mutates document state.
- `DocumentController` is GUI-thread confined and owns its store and
  `QFileSystemWatcher` for its lifetime. It publishes complete state changes
  and never chooses overwrite, reload, or discard consent.
- `EditorWindow` owns standard actions, menus, dialogs, focus, accessible
  descriptions, and the external-change alert. It never accesses files
  directly.

All expected errors cross these boundaries as typed values plus bounded user-
facing diagnostics. An unsuccessful open or save preserves the last complete
document state. There is no background worker, service, D-Bus authority, shell
private dependency, or exception-based failure channel in S1.

The document/support C++ headers and build target are private implementation
surfaces and are not installed or ABI-stable. The executable name, desktop ID,
single-file launch contract, standard shortcut meanings, and documented action
object names form the S1 compatibility surface. A breaking command or desktop
identity change requires an explicit application-contract update; internal
collaborators may change atomically with their tests and this page.

## QST-1 theme and accessibility boundary

The presentation adapter derives its complete `QPalette`, interface font,
monospace editor font, focus ring, surfaces, outlines, and text colors from the
public C++ QST-1 boundary. The editor does not import shell or Controls internals
and contains no fallback brand palette. `qinda-dark` is the launch default;
`--theme` and an optional local `--theme-directory` select a validated schema-v1
theme. A theme whose validated variant is `high-contrast` explicitly enables
QST-1's caller-owned high-contrast derivation input. Settings1-backed selection
and live theme replacement remain later application-composition work.

The supported `--check-theme` diagnostic resolves the selected installed theme,
derives its complete QST revision, prints the resulting theme/revision identity,
and exits before constructing a window. Packaging uses it to prove that source-
tree data and build-tree linkage are not masking a broken installed prefix; the
installed gate exercises all five built-in themes and reads the installed
desktop entry rather than its source copy. The focused `TextEditor` install
component contains the executable, desktop entry, required built-in theme
data, and the `QindaQt.AppShell`, `QindaQt.Controls`, and `QindaQt.Tokens`
shared runtime libraries needed by that executable's verified relative RUNPATH
chain. Unrelated application binaries and QML plugins remain excluded, so the
packaging proof does not depend on them.
`--report-startup` emits the elapsed milliseconds only after the real top-level
has completed its first paint; it does not bypass document loading or theme
composition.

The document editor receives initial focus and has an explicit accessible name
and description. External-change text and both recovery buttons are named, and
showing the banner emits an assertive accessibility announcement containing the
complete current warning only when external state transitions, not when local
dirty state changes beneath an already-visible warning. If a successful recovery
hides a focused banner action, focus returns to the document editor. Standard
dialogs preserve keyboard-only Save/Discard/Cancel and replacement decisions.

## Desktop integration and verification

`org.qindaqt.TextEditor.desktop` registers the ordinary Wayland application for
`text/plain` with one `%f` local-file argument. Multiple file arguments are
rejected rather than silently dropping documents.

The focused selector is:

```sh
ctest --test-dir build/dev -R '^qindaqt\.editor-' --output-on-failure
```

It covers exact dirty-state transitions, incremental edit projection,
BOM/UTF-8/size failures, atomic save and conflict refusal (including same-path
Save As consent), controller recovery, standard menus/shortcuts,
accessibility/focus metadata, external-change presentation, transition-only
announcements, desktop metadata, and command-line arity. A dedicated 8 MiB row
records bounded open, end-edit, and atomic-save behavior with 5 s open/save and
500 ms incremental-edit ceilings. Installed-prefix launch, package contents,
the affected theme/QST public-dependency rows in Debug and Release, and an
installed offscreen process probe are all
mandatory before S1 can be called qualified. The initial editor-specific hard
gates are at most 400 ms from process start to first painted frame and at most
64 MiB median PSS across five settled samples. Candidate handoffs record the
actual samples; the probe launches no host surface, nested compositor, input
driver, or session integration.

## Bounded S1 deferrals

- Tab remains an editor character. Visible recovery buttons retain direct Alt
  mnemonics and reverse focus traversal; a forward pane-cycle shortcut waits for
  a desktop-wide focus convention rather than becoming editor-private policy.
- File selection is now injectable through AppShell's portal request and
  `FileSelectionAdapter` (see [AppShell participation](#appshell-participation)),
  but the dirty-save and replace-confirmation consent dialogs remain direct,
  non-injectable `QMessageBox` calls. Controller tests prove every
  destructive state transition; an injectable consent seam waits until a
  second application proves reusable demand.
- The desktop entry uses the platform's `accessories-text-editor` icon. A
  QindaQt-branded installed icon belongs to a later branding asset slice.
