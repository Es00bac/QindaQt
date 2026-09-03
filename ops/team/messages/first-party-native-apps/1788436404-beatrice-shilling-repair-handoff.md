# Beatrice Shilling — Text Editor S2 repair handoff

- Timestamp: 2026-09-03T05:53:24-06:00
- Exact repaired candidate: `d68f8b1576db9e0e185ff203a883721203197116`
- Candidate tree: `46f03841b67fadefb525fa2cdef1f9f3862d10cb`
- Candidate parent / prior coordination head: `352929ba701ff9e4ca72c4ab1eb4c360121e7bc5`
- Rejected candidate: `a13aa620f7ad1a4756b8a5e2ab8ae7887244d0b3`
- Exact original product base: `b2f515986150b1acfe82e2807a78a731a58a94a2`
- Branch/worktree: `worker/text-editor-s2`, `/home/cabewse/work_SPaC3/container-wm-workers/text-editor-s2`

## Finding closure

- P1-1 closes in `d68f8b1576db9e0e185ff203a883721203197116`: `DocumentController::normalizePath` now resolves a prospective target through its canonical parent and refuses an unresolved parent or dangling final symlink. `DocumentCollectionTest::saveAsResolvesSymlinkedParent` is the named regression; it proves Save As through `alias/` adopts the canonical real file, reopening the real path focuses the one existing controller, and an uncanonicalizable parent returns `InvalidPath` without creating a file.
- P1-2 closes in `d68f8b1576db9e0e185ff203a883721203197116`: `RestoreStateStore` walks every absolute directory component using `openat` with `O_NOFOLLOW`, creates missing components through `mkdirat`, uses the retained final descriptor for `QSaveFile` commit, and applies descriptor-relative no-follow load/clear operations. `RestoreStateTest::symlinkedAncestorCannotEscapeStateRoot` is the named regression; it requires `InvalidRoot` and no file beneath the escape target.
- P2-1 closes in `d68f8b1576db9e0e185ff203a883721203197116`: the sanitizer admits collapsed whitespace plus its following scalar only when their combined UTF-16 cost fits. `DocumentCollectionTest::titleCollapseHonorsUtf16Boundary` is the named regression and covers the former 129-unit scalar case plus exact 128-unit and refused-overflow surrogate-pair cases.
- P2-2 closes in `d68f8b1576db9e0e185ff203a883721203197116`: the two CLI rows and installed-package row register `QT_FATAL_WARNINGS=1`. The new registered `qindaqt.editor-offscreen-warning-policy` row introspects the generated CTest registry; its checker exits 1 against the rejected candidate registry and passes in both repaired profiles.

## Repair changed paths

- `docs/wiki/adr/0064-persist-text-editor-path-inventory.md`
- `docs/wiki/apps/text-editor.md`
- `docs/wiki/development/testing-harness.md`
- `src/apps/text_editor/document/document_controller.cpp`
- `src/apps/text_editor/restore/restore_state_store.cpp`
- `src/apps/text_editor/restore/restore_state_store.h`
- `src/apps/text_editor/ui/document_title.cpp`
- `tests/apps/text_editor/CMakeLists.txt`
- `tests/apps/text_editor/check_registered_environment.py`
- `tests/apps/text_editor/tst_document_collection.cpp`
- `tests/apps/text_editor/tst_restore_state.cpp`

## Reproduction evidence against `a13aa62`

- `/home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/repros/build/exact_candidate_repros save-as-alias` — exit 1 as designed: stored alias path, real-path reopen admitted a second controller.
- Same executable with `state-parent-alias` — exit 1 as designed: store returned success and created the escaped inventory.
- Same executable with `title-boundary` — exit 1 as designed: title size was 129 UTF-16 units.
- `python3 tests/apps/text_editor/check_registered_environment.py --ctest "$(command -v ctest)" --build-directory /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/dev` — exit 1 as designed: `qindaqt.editor-cli-hostile-argv` lacked `QT_FATAL_WARNINGS=1` (the exact review registry also showed all three missing properties).

## Build and test evidence

- Exact common-brief Debug configure command with `-B /home/cabewse/work_SPaC3/builds/qindaqt/text-editor-s2/debug`, the 6.6.5 initial cache, Debug, testing/plugin/shell/production-shell enabled, host uinput disabled, and strict warnings enabled — exit 0. CMake emitted the repository's known runtime-search-path warnings.
- Exact Release configure with the same options and `-B .../release -DCMAKE_BUILD_TYPE=Release` — exit 0 with the same known configure warnings.
- `cmake --build .../debug --parallel 3 --target qindaqt-editor qindaqt_editor_document_state_tests qindaqt_editor_local_store_tests qindaqt_editor_controller_tests qindaqt_editor_large_document_tests qindaqt_editor_document_collection_tests qindaqt_editor_find_replace_tests qindaqt_editor_restore_state_tests qindaqt_editor_restore_policy_tests qindaqt_editor_window_tests qindaqt_editor_app_shell_tests qindaqt_settings_schema_tests qindaqt_settings_migration_tests` — exit 0, 26/26 rebuilt actions in the incremental strict Debug tree.
- Release equivalent — exit 0, 34/34 rebuilt actions in the incremental strict Release tree.
- Final immutable-candidate Debug `ctest -R '^qindaqt\.editor-' --output-on-failure --no-tests=error` under `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent` with HOME/XDG_STATE_HOME/XDG_DATA_HOME/TMPDIR below `/home/cabewse/work_SPaC3/builds/qindaqt/text-editor-s2/scratch-debug-candidate` — exit 0, 16/16 passed.
- Final immutable-candidate Release equivalent beneath `scratch-release-candidate` — exit 0, 16/16 passed.
- Final immutable-candidate Debug `ctest -R '^qindaqt\.settings-(schema|migration)$'` under the same isolation — exit 0, 2/2 passed.
- Final immutable-candidate Release equivalent — exit 0, 2/2 passed.

## Static evidence

- `./tools/validate-docs` — exit 0, 129 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/text-editor-s2/site-candidate` — exit 0.
- `./tools/check-source-shape` — exit 0, 2,083 files checked and 0 skipped; it reported the four existing out-of-lane decomposition-review warnings (`tst_settings_navigation_page.cpp`, `tests/compositor/CMakeLists.txt`, `tst_color_model.cpp`, and `tst_audio_applet_controller.cpp`).
- `git diff --check a13aa620f7ad1a4756b8a5e2ab8ae7887244d0b3..d68f8b1576db9e0e185ff203a883721203197116` — exit 0.
- `python3 -m json.tool data/settings/schema-v1.json > /dev/null` and schema-v2 equivalent — each exit 0.

## Bounded caveats and next action

This repair claims deterministic local filesystem, process-local Qt, offscreen warning-fatal, package, Settings fake/disconnected-transport, and documentation evidence only. It did not start a compositor, nested or host desktop, host D-Bus service, hardware, uinput, network, real portal/global menu, or whole-application assistive-technology bridge. The inherited S2 exclusions for portals beyond the injected chooser seam, print, extensions, rich text, content journals/autosave, nested screenshots, and physical display/input remain unchanged.

Requested next action: independent exact re-review by Olga Ladyzhenskaya of `d68f8b1576db9e0e185ff203a883721203197116`, then Program Manager integration if accepted.
