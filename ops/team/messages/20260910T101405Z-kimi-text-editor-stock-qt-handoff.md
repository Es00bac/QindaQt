# Text Editor stock-Qt conversion + printing + crash-recovery autosave — handoff

**Worker:** kimi (Text Editor lane)
**Base:** work committed directly on `main` in the shared checkout per manager instruction
**Commits:**

1. `504ff587` — "Build the Text Editor on stock Qt 6 Widgets presentation" (ADR-0116 de-chrome: token→palette projection, application QSS chrome, and per-app theme catalog removed; palette/fonts/icon theme from the Qt platform theme with Fusion; `--theme`/`--theme-directory` kept as deprecated no-ops for external harnesses; 24/24 rows green at the committed tree).
2. `942b13e3` — "Add Text Editor printing and crash-recovery autosave" (31 files, +1573/−115).

## What landed in 942b13e3

- **Printing** (`ui/editor_window_printing.cpp`): `file.print` (`Ctrl+P`) and `file.print-preview` (`Ctrl+Shift+P`) actions in the File menu and the AppShell catalog (now 27 entries), enabled only while the document has content. Both native dialogs share one pipeline: a fresh `QTextDocument` of the controller's plain text in `DocumentEditor::baseFont()` — deliberately no syntax colors, since the highlight palette is fitted to the screen, not a white page (AGENT-CONTRACT in the source). `EditorWindow::printDocumentToPdfFile` is the public dialog-free seam over the same renderer.
- **Crash-recovery autosave** (`restore/recovery_journal_store.{h,cpp}`, `ui/editor_window_recovery.cpp`): one bounded JSON journal per document (`{"version":1,"path":<string|null>,"text":<string>}`), 4 MiB text cap, 32-journal cap, never truncated — an oversized document keeps its stale earlier journal so recovery still offers the last bounded state. Keys are `file-<sha256(canonical path)>` / `untitled-<pid>-<sequence>`; writes are atomic owner-only `QSaveFile` replacements beneath `<state>/qindaqt/text-editor/recovery`, reached through the `openat`/`O_NOFOLLOW` walk now shared with the restore inventory as `restore/state_directory.{h,cpp}` (extracted from `restore_state_store.cpp`, no behavior change there).
- **Write/clear wiring:** writes hook the editor's `textChanged`, *not* controller `stateChanged` (edits only emit `stateChanged` on dirty flips and `openPath` emits it unconditionally — hooking it would erase stale journals before consent); the first dirty edit journals immediately, later churn debounces at 2000 ms. Clears hook `stateChanged` only on a tracked dirty→clean transition (save, reload, undo-to-clean). Discard close-consent clears and ends journaling for that window.
- **Consent, never silent:** opening a journaled document prompts Restore (default, non-destructive) / Discard (clears, denies re-journaling for that window); a journal matching disk content retires without prompting; a startup sweep offers untitled orphans once per launch and re-journals adopted content under the new window's own key before retiring the orphan. Malformed/foreign journals stay inert on disk. `DocumentDialogs` gained the `recoverUnsaved` seam (native `QMessageBox`, Restore default, Discard destructive).

## Defect found and fixed during verification (worth reviewer attention)

**Destruction-order assert on `textChanged`:** `~QSyntaxHighlighter` detaches from the `QTextDocument` and flushes a pending `textChanged` while the view subtree is destroyed from `~QWidget` — at that point the `EditorWindow`'s dynamic type is already the base class, so the typed member-function connection fatally asserted ("Called object is not of the correct type"). This fired in the new recovery row *and* in the four real-process global-menu private-bus rows. `~EditorWindow` now severs the `textChanged`→`scheduleRecoveryWrite` connection while the body still runs and `m_view` lives (AGENT-GUARD in `editor_window_recovery.cpp`). The same hazard shape applies to any typed slot connected to a signal a child can emit during destruction.

## Changed paths

- `src/apps/text_editor/**` (new: `restore/state_directory.*`, `restore/recovery_journal_store.*`, `ui/editor_window_printing.cpp`, `ui/editor_window_recovery.cpp`; modified: controller `restoreRecoveredText`, dialogs recovery seam, application ctor/sweep, window ctor/dtor/wiring, action catalog, CMake PrintSupport, `main.cpp` composition)
- `tests/apps/text_editor/**` (new rows `tst_editor_printing.cpp`, `tst_recovery_journal.cpp`, `tst_editor_recovery.cpp`; catalog count and scripted-dialog ripples)
- `docs/wiki/apps/text-editor.md` (printing + autosave sections, ADR-0116 appearance rewrite replacing the stale QST-1/ADR-0080/`--check-theme` prose), Text Editor progress paragraph in `docs/TASK_LIST.md`

## Gates (all at committed tree `942b13e3`)

- Full `cmake --build build/dev -j8` green under `-Werror`.
- `ctest --test-dir build/dev -R '^qindaqt\.editor'`: **27/27, run twice** (24 existing + new `qindaqt.editor-printing-offscreen`, `qindaqt.editor-recovery-journal`, `qindaqt.editor-recovery-offscreen`).
- `python3 tools/check-source-shape`: no errors in lane paths; only the pre-existing `tst_editor_menu_export.cpp` 518-line decomposition WARNING remains (not introduced here).
- `.cache/handbook-docs-venv/bin/mkdocs build --strict` and `python3 tools/docs_validation.py` both pass.

## Caveats / bounded limits

- Discard-deny is per-window and session-scoped: re-opening the same path in a new window re-offers a journal only if one still exists.
- Cross-process concurrency on one journal key is last-writer-wins by design (documented in the store's AGENT-CONTRACT); no locking is implied.
- Recovery dialog coverage is through injected scripted dialogs; the native `QMessageBox` adapter itself is construction-level only (same standing pattern as close consent).
- `docs/wiki/architecture/font-preferences.md` still lists Text Editor in its F1 per-app font section; that file is outside this lane's ownership and should be re-scoped by its owner to match ADR-0116.

**Requested next action:** review/integrate commits `504ff587` and `942b13e3` on `main`; no rebase needed (already on main). Text Editor lane is clear for the next outcome.
