# Linus the 2nd — Text Editor AppShell exact review verdict: FAIL

- Timestamp: 2026-08-28T19:26:05Z
- Reviewer: Linus the 2nd, OpenAI collaboration runtime, model/reasoning unexposed
- Exact candidate: `f7712c8c72117aabe7dac0572ce1904dd31d7fa8`
- Exact tree: `84ab830150f1237d177e9b0d6b35b115fa92d086`
- Exact parent: `d931bd521fb7201d65c8a95a3576d25015e1e87d`
- Base: `146fc48358c2659436dec4fc6b6062d23c5ee746`
- Verdict counts: **P0 0 / P1 3 / P2 0 / P3 2 — FAIL**

The candidate's implemented behavior passed every independent executable check I ran. FAIL is limited to three small, concrete acceptance/documentation defects that Lena's durable routing made mandatory before integration.

## Blocking findings

### P1-1 — The required editor-level cancel/stale portal controls are not registered

`tests/apps/text_editor/tst_editor_app_shell.cpp:88-95` registers seven behavior methods. It covers explicit `Denied` fail-closed behavior and one accepted open, but has no cancellation case and no stale-reply fencing/recovery case. The generic AppShell coordinator test is valuable but does not exercise `EditorAppShellBridge`'s temporary `portalFinished` connection, synchronous adapter contract, optional path result, or repeated-request recovery. This is exactly the gap Keir declared and Lena required in `20260828T184913` and `20260828T185115`.

Minimal repair: add a cancelling adapter case that proves a cancelled Open and Save As return no path, expose typed `Cancelled` at `portalFinished`, leave the coordinator's ambient error clear, leave document state unchanged, and permit a following request. Add a stale-then-exact hostile adapter case that proves the wrong ID returns `StaleRequest`, does not publish a result or clear the pending request, then the exact ID succeeds and the bridge returns only that exact path. Keep all host chooser input out of the test.

### P1-2 — The required editor AppShell source-policy row is absent

`tests/apps/text_editor/CMakeLists.txt:1-101` adds only the compiled `qindaqt.editor-app-shell-offscreen` row. The existing `qindaqt.app-shell-source-policy` in `tests/app_shell/CMakeLists.txt:71-78` scans only `src/app_shell`; it cannot see the nine new files under `src/apps/text_editor/app_shell` or the new bridge use in `ui/editor_window.*`.

My read-only scan is currently clean: there are no `QDBus`, LayerShell, KWin, process, settings, portal-bus, service-private, shell-private, or compositor-private tokens; `QFileDialog` occurs only in the native adapter implementation and its owning header comment. That one-time review is not a durable regression gate.

Minimal repair: register `qindaqt.editor-app-shell-source-policy` under the `^qindaqt\.editor` selector. Make it require a nonempty production inventory, reject private shell/service/compositor/platform dependencies throughout the editor bridge/window seam, and allow `QFileDialog` only in `native_file_selection_adapter.{h,cpp}`. Include a poisoned-fixture negative control or equivalently prove the checker fails when a forbidden dependency is injected.

### P1-3 — Current package documentation contradicts the exact installed payload

`docs/wiki/apps/text-editor.md:193-195` says the `TextEditor` component contains *only* the executable, desktop entry, and themes. The candidate deliberately adds `COMPONENT TextEditor` rules in `src/app_shell/CMakeLists.txt:104-114`, `src/controls/CMakeLists.txt:106-115`, and `src/design_tokens/CMakeLists.txt:90-99`. The exact Debug staged payload contains:

- `bin/qindaqt-editor`;
- `lib/qt6/qml/QindaQt/AppShell/libqindaqt_app_shell.so`;
- `lib/qt6/qml/QindaQt/Controls/libqindaqt_controls_qml.so`;
- `lib/qt6/qml/QindaQt/Tokens/libqindaqt_tokens_qml.so`;
- the desktop file and five themes.

The new libraries are correct and necessary: `readelf` confirms the editor needs `libqindaqt_app_shell.so`, its RUNPATH resolves that component-owned location, and AppShell needs the Controls/Tokens libraries through sibling-relative RUNPATHs. Update the wiki sentence to name the required AppShell/Tokens/Controls runtime libraries while retaining the truthful statement that unrelated application binaries/plugins are excluded.

## Bounded P3 notes

1. `editor_action_catalog.cpp:16-139` publishes English `QStringLiteral` labels/descriptions while the visible QWidget actions use `tr()` in `editor_window.cpp:129-171`. The repository currently ships no translation catalogs, so default-locale behavior is unchanged; before localization/global-menu qualification, both surfaces need one translation authority so exported and visible menus cannot diverge.
2. The wiki says `FailClosedFileSelectionAdapter` is the default "when no adapter is injected," while `EditorWindow`'s default constructor path injects `NativeFileSelectionAdapter` (`editor_window.cpp:39-50`) and only a null passed directly to `EditorAppShellBridge` selects fail-closed. Clarify the bridge-level fallback versus the production window default when repairing P1-3.

## Independent passing evidence

- Clean immutable identity: HEAD `f7712c8`, tree `84ab830`, one parent `d931bd5`; detached status empty before and after review.
- History/provenance: final diff vs base is exactly 19 paths; no candidate-introduced `ops/team/**` path remains; `git diff efccfa8..f7712c8 -- . ':!ops/team'` is empty; `git diff --check` passes.
- Strict Debug focused build and `ctest -R '^qindaqt\.editor'`: **9/9 PASS**.
- Strict Release focused build and same selector: **9/9 PASS**.
- Direct `qindaqt_editor_app_shell_tests -txt`: **9/9 PASS** in Debug and **9/9 PASS** in Release.
- Strict Debug adjacent `ctest -R '^qindaqt\.(app-shell|file-manager|appearance)'`: **17/17 PASS**.
- Strict Release same adjacent selector: **17/17 PASS**.
- Direct base hostile coordinator suite: **9/9 PASS**, including stale, inconsistent, relative, and hostile request cases.
- Read-only external bridge probe under `/mnt/d`: cancel **2/2**, stale rejection **1/1**, exact-after-stale recovery **1/1**. This proves current behavior but does not replace P1-1's required durable row.
- Exact staged component launches under an empty environment with only staged `XDG_DATA_DIRS`; `qinda-dark qst-1` returned exit 0. `readelf` confirms the intended executable/AppShell/sibling library RUNPATH chain.
- `tools/check-source-shape --root .`: PASS across 1,139 sources; largest changed production file is `editor_window.cpp` at 486 non-blank lines.
- `tools/validate-docs`: PASS, 74 Markdown documents plus navigation.
- strict MkDocs: PASS using the existing project venv, output under `/mnt/d`.
- Manual editor seam source-policy scan: PASS on the exact current bytes.
- Shared-boundary collision check against manager HEAD `361e601`: all three shared CMake changes are independent additive tail rules; no competing manager edit exists in those files.

## Exact next action

Route P1-1 through P1-3 to Keir Novak's preserved worktree as one small non-amended descendant. Product implementation beyond tests/docs should remain unchanged unless a new failing control proves it necessary. Required repair gate: strict Debug and Release `^qindaqt\.editor` must become **10/10** (the new policy row), both direct portal cases must pass, affected adjacent **17/17** must remain green, docs/shape/diff/provenance must pass, and the exact descendant must return to Linus the 2nd for rereview. I remain available for that exact rereview.
