# Keir the 2nd — Text Editor AppShell exact repair handoff

- Timestamp: 2026-08-28T19:44:41Z
- Exact commit: `75f786e91a1877b9eb9fa0e2750fc2ddac1a9d80`
- Exact tree: `65ea635651674dac73106019d61a845096d24280`
- Sole parent: `f7712c8c72117aabe7dac0572ce1904dd31d7fa8`
- Branch: `worker/text-editor-appshell-repair-keir2`
- Worktree: `/mnt/d/QindaQt/worktrees/text-editor-appshell-repair-keir2`
- Verdict requested from: Linus the 2nd, retained exact reviewer

## Outcome

This four-path non-amended descendant closes Linus's three P1 findings without
changing production behavior:

- `tst_editor_app_shell.cpp` now drives typed Open and Save As cancellation
  through `EditorWindow`, proves no document mutation or ambient error, and
  proves a later request reaches the adapter. Its hostile adapter submits the
  wrong request ID first, records `StaleRequest`, observes no stale publication,
  then resolves the still-pending exact ID and opens only the exact path.
- `qindaqt.editor-app-shell-source-policy` scans all nine editor AppShell
  bridge/adapter files plus `editor_window.{h,cpp}`. It rejects D-Bus,
  LayerShell/KWin/process/settings/portal and private service/shell/compositor/
  platform dependencies, confines `QFileDialog` to
  `native_file_selection_adapter.{h,cpp}`, and invokes the exact checker on a
  generated QDBus-poisoned fixture that must fail.
- The Text Editor wiki now distinguishes `EditorWindow`'s native production
  default from `EditorAppShellBridge`'s null-adapter fail-closed fallback and
  names the staged AppShell/Controls/Tokens libraries required by the verified
  relative RUNPATH chain.

Exact changed paths (path-list SHA-256
`b62cff4fa25f185daef2f54187b96eb786f19e76710f66d5f677e8e78b0c981f`):

- `docs/wiki/apps/text-editor.md`
- `tests/apps/text_editor/CMakeLists.txt`
- `tests/apps/text_editor/check_editor_app_shell_source_policy.cmake`
- `tests/apps/text_editor/tst_editor_app_shell.cpp`

## Executed evidence

- Strict Debug configure/build, explicit seven editor targets: exit 0.
- Strict Release configure/build, same explicit targets: exit 0.
- Debug `ctest -R '^qindaqt\.editor' --no-tests=error`: **10/10 PASS**.
- Release same selector: **10/10 PASS**.
- Direct Debug `qindaqt_editor_app_shell_tests -txt`: **11/11 PASS**,
  including both new portal controls.
- Direct Release same: **11/11 PASS**.
- Explicit Debug adjacent targets followed by
  `ctest -R '^qindaqt\.(app-shell|file-manager|appearance)'`: **17/17 PASS**.
- Explicit Release same: **17/17 PASS**.
- The editor staged-component row passes in both configurations. The exact
  stage contains the executable, desktop entry, five themes, and only the
  three named QindaQt shared libraries; `readelf` confirms the executable's
  AppShell RUNPATH and AppShell's sibling Tokens/Controls RUNPATH.
- `tools/check-source-shape --root .`: PASS, 1,140 source files; largest
  changed production source remains `editor_window.cpp` at 486 nonblank lines.
- `tools/validate-docs`: PASS, 74 Markdown documents plus navigation.
- `mkdocs build --strict`: PASS to an external `/mnt/d` site directory.
- `git diff --check`, one-parent ancestry, exact four-path manifest and final
  `git status --porcelain`: clean.

## Bounded caveats and next action

Linus's two nonblocking P3 boundaries remain intentionally unexpanded: action
catalog localization still needs one later translation authority, and this is
not evidence for a real portal transport/global-menu export/Settings or session
composition. Please rereview exact immutable commit `75f786e…`; if it passes,
the Program Manager can integrate this preserved candidate and rerun the
combined tree gate.
