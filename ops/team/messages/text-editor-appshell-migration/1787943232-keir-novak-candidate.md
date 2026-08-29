# Candidate: Text Editor migrated onto QindaQt.AppShell 1.0

- Worker: Keir Novak (Anthropic Claude Code, `claude-sonnet-5`, high)
- Commit: pending (this reply is posted just before the candidate commit; the
  next reply in this thread will name its exact SHA once created).
- Outcome delivered: `EditorWindow` now owns a GUI-thread
  `ApplicationCoordinator`, publishes the documented File/Edit action set as
  one atomic `ActionSpec` batch through `ActionRegistry`, routes close
  consent through `requestQuit`/`resolveQuit` instead of a raw `closeEvent`
  dirty-prompt, and mediates Open/Save As through a `FileSelectionAdapter`
  seam (`NativeFileSelectionAdapter` default, `FailClosedFileSelectionAdapter`
  for tests) behind `requestOpenFile`/`requestSaveFile`. New files under
  `src/apps/text_editor/app_shell/` and `tests/apps/text_editor/
  tst_editor_app_shell.cpp`. Docs updated:
  `docs/wiki/apps/text-editor.md` (bounded-deferral bullet corrected: file
  selection is now injectable, consent dialogs remain direct Qt calls) and
  `docs/wiki/apps/application-shell.md` (names Text Editor as the first
  migrated consumer).

- **Coordination-point edits outside my owned paths** (flagging per AGENTS.md
  "Shared registries and build files are coordination points"):
  - `src/app_shell/CMakeLists.txt`: added an additive
    `install(TARGETS qindaqt_app_shell LIBRARY ... COMPONENT TextEditor)`
    rule. `qindaqt-editor` links `QindaQt::AppShell`'s C++ classes directly
    (not via QML import), so it has a real ELF `DT_NEEDED` on
    `libqindaqt_app_shell.so` that the library's existing install() rule
    (no COMPONENT at all) never staged for any narrow single-component
    install. Nothing existing was changed or removed.
  - `src/design_tokens/CMakeLists.txt` and `src/controls/CMakeLists.txt`:
    added matching additive `LIBRARY`-only `COMPONENT TextEditor` installs
    for `qindaqt_tokens_qml`/`qindaqt_controls_qml`, the shared libraries
    `qindaqt_app_shell` privately links. Mirrors the existing
    `SettingsAppearanceRuntime` per-consumer COMPONENT precedent already in
    both files rather than reusing/renaming it (reusing it directly pulled
    in Settings Appearance's own unrelated, unbuilt QML plugin target and
    failed the staged install for an unrelated reason).
  - Verified no regression: full Debug and Release re-runs of
    `qindaqt.app-shell-*` (5/5), `qindaqt.file-manager-*` (8/8), and
    `qindaqt.appearance-*` (4/4) all still pass after these edits, alongside
    the 9/9 `qindaqt.editor-*` rows.

- **Verification gates run** (both `build/dev` [Debug] and `build/release`
  [Release], `QINDAQT_ENABLE_STRICT_WARNINGS=ON`, clean reconfigure from
  scratch after the shared-worktree build directory was relocated to
  `/mnt/d/QindaQt/builds/text-editor-appshell-claude-keir` mid-session):
  - `ninja qindaqt-editor` and the full `qindaqt_editor_support` object set:
    clean, zero warnings, in both configurations.
  - `ctest -R '^qindaqt\.editor'`: 9/9 passed in Debug, 9/9 in Release
    (`editor-document-state`, `editor-local-store`, `editor-controller`,
    `editor-large-document`, `editor-window-offscreen`,
    `editor-app-shell-offscreen`, `editor-desktop-metadata`,
    `editor-cli-rejects-multiple-paths`,
    `editor-installed-theme-and-metadata`).
  - `ctest -R '^qindaqt\.(app-shell|file-manager|appearance)'`: 17/17 passed
    in Debug, 17/17 in Release (adjacent-module regression check for the
    three coordination-point files above).
  - `tools/validate-docs`: 74 Markdown documents + mkdocs.yml navigation
    validated, clean.
  - `mkdocs build --strict`: clean, zero warnings.
  - `git diff --check`: clean (no trailing whitespace / EOF issues) across
    every changed and new file.

- **Environment note, not a product defect**: the worktree's `build` symlink
  was relocated to `/mnt/d/QindaQt/builds/text-editor-appshell-claude-keir`
  mid-session (desktop crash recovery). A fresh CMake configure through that
  new location initially failed on `qindaqt_themes` (unrelated to this
  outcome) with a moc-generated relative `#include` resolving to the wrong
  physical location; a from-scratch `rm -rf` of the stale build tree plus
  reconfigure resolved it cleanly with no source change required. Flagging
  in case other worktrees hit the same symptom after the same relocation.

- Remaining bounded caveats (unchanged from the wiki, not introduced by this
  candidate): consent `QMessageBox` dialogs stay direct, non-injectable Qt
  calls (documented deferral, second-application-demand gate); Tab-as-pane-
  cycle and branded install icon deferrals are pre-existing and untouched.

- Requested next action: a different worker review this exact commit
  (paths: `docs/wiki/apps/application-shell.md`,
  `docs/wiki/apps/text-editor.md`, `src/app_shell/CMakeLists.txt`,
  `src/apps/text_editor/**`, `src/controls/CMakeLists.txt`,
  `src/design_tokens/CMakeLists.txt`, `tests/apps/text_editor/**`) before
  manager integration.
