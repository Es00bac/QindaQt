# Olga Ladyzhenskaya — Text Editor S2 repair recheck

- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `d68f8b1576db9e0e185ff203a883721203197116`
- Tree SHA: `46f03841b67fadefb525fa2cdef1f9f3862d10cb`
- Parent SHA: `352929ba701ff9e4ca72c4ab1eb4c360121e7bc5`
- Base SHA: `b2f515986150b1acfe82e2807a78a731a58a94a2`
- Rejected ancestor: `a13aa620f7ad1a4756b8a5e2ab8ae7887244d0b3`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/text-editor-s2-codex-review`
- Review build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex`

The candidate, tree, parent, and original base identities matched before and
after review. `git status --porcelain` was empty at both boundaries. I did not
edit, commit, amend, rebase, or otherwise mutate a product path. All added
fixtures and extracted ancestor sources stayed beneath the assigned review
build root. I did not run `tests/session`, a compositor, a host D-Bus service,
hardware, uinput, or network access.

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Prior-finding closure

### P1-1 — canonical Save As identity

Closed. `DocumentController::normalizePath()` at
`src/apps/text_editor/document/document_controller.cpp:28` now rejects an
unresolvable/dangling target and derives a prospective file from its canonical
parent. The registered
`DocumentCollectionTest::saveAsResolvesSymlinkedParent` assertions at
`tests/apps/text_editor/tst_document_collection.cpp:89` require the stored real
path, one controller after reopening the real path, and `InvalidPath` without a
file for an unresolvable parent. They pass in Debug and Release.

The original scratch reproduction now exits 0 with `stored-path` equal to the
real canonical path, `focused-existing=1`, and `document-count=1`. Additional
neutral fixtures also exit 0: a two-link parent chain preserves the same one-
controller result, and replacing a prospective destination parent with a link
after the source document is open resolves and stores the replacement's real
path. A separate opened-document parent replacement containing different bytes
returns `ExternalConflict`, leaves the replacement unchanged, and retains the
local edit.

Against source extracted from exact ancestor `a13aa62`, the same single-link,
chain, and parent-change controls each exit 1: the stored path remains the link
spelling, and reopening the real path creates a second controller in the first
two cases. This confirms the registered repair assertions distinguish the
rejected implementation rather than merely passing both versions.

### P1-2 — restore state-root containment

Closed. `RestoreStateStore` walks every absolute directory component with
`openat(..., O_NOFOLLOW)`, retains the final directory descriptor through
`QSaveFile::commit()`, opens load targets descriptor-relatively, and clears with
`unlinkat()`. The registered
`RestoreStateTest::symlinkedAncestorCannotEscapeStateRoot` at
`tests/apps/text_editor/tst_restore_state.cpp:89` requires `InvalidRoot` and no
inventory beneath the redirected path; it passes in both profiles.

The original control now exits 0 with `store-ok=0`, `store-error=2`
(`InvalidRoot`), and `escaped-file-exists=0`. The same control against
`a13aa62` exits 1 with a successful store and an inventory beneath the linked
ancestor target.

I also accepted the descriptor-retention rebuttal for the requested path-change
window after executing it. A scratch watcher changed an already-open ancestor
path to a link immediately after `QSaveFile` created its same-directory
temporary entry. The production store succeeded through the retained
descriptor, the inventory appeared beneath the renamed original directory, and
no file appeared beneath the link target (`ancestor-changed=1`, `store-ok=1`,
`outside-file-exists=0`, `retained-file-exists=1`). This is the expected
descriptor-fenced behavior rather than a lexical-path write.

### P2-1 — 128-unit title limit

Closed. `sanitizeDocumentTitle()` at
`src/apps/text_editor/ui/document_title.cpp:21` budgets a pending collapsed
space and the following scalar together before appending either. The registered
`titleCollapseHonorsUtf16Boundary` assertions at
`tests/apps/text_editor/tst_document_collection.cpp:156` cover the former
129-unit case, an exact 128-unit supplementary-scalar case, and refusal of an
overflowing supplementary scalar without producing malformed UTF-16. They pass
in Debug and Release.

The original 127-unit-prefix/space/scalar control now reports `title-size=127`
and valid UTF-16, exit 0. The same control against `a13aa62` reports
`title-size=129`, exit 1.

### P2-2 — fatal-warning registration

Closed. `tests/apps/text_editor/CMakeLists.txt:148`, `:158`, and `:182`
register `QT_FATAL_WARNINGS=1` for both CLI script rows and the installed
package row. The registered
`qindaqt.editor-offscreen-warning-policy` checker enumerates the CTest registry,
requires all three named rows to exist, and requires the exact environment
entry. It passes in both repaired profiles. Against the preserved ancestor
`dev` registry it exits 1 and names
`qindaqt.editor-cli-hostile-argv does not register QT_FATAL_WARNINGS=1`.

## Regression evidence

- The registered collection assertions preserve independent per-document
  external state; focused Debug and Release executions pass.
- `DocumentControllerTest::externalReplacementBlocksSave` passes in both
  profiles. It observes `Changed`, requires `ExternalConflict`, retains the
  local text, then repeats the refusal for a missing file.
- `EditorWindowTest::externalChangeShowsNonDestructiveBanner` passes in both
  profiles with `QT_FATAL_WARNINGS=1`; the warning is tab-local, textual, and
  non-destructive for changed, missing, and unreadable states.
- `RestorePolicyTest::baselineAndLossFailClosed` and
  `startupDropsUnavailablePathsWithoutContentState` pass in both profiles,
  proving fallback to disabled on lineage loss and paths-only rewrite after a
  skipped target.
- A clean disconnected offscreen Debug launch reported
  `startup-first-frame-ms=45`; the one-second wrapper then exited 124 as
  intended. Its fresh redirected state root contained zero files and zero
  entries, confirming unavailable/disabled restore creates no inventory or
  directory.

## Commands and results

### Identity and diff inspection

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse b2f515986150b1acfe82e2807a78a731a58a94a2
git status --porcelain
git log --oneline --decorate --graph --max-count=12
git diff --name-status a13aa62..d68f8b1
git diff --stat a13aa62..d68f8b1
git diff --name-status b2f5159..d68f8b1
```

Result: all commands exit 0. Identities match the header; status is empty. The
repair contains 15 changed paths including additive coordination records; the
whole candidate contains 55 changed paths. The 11 product/test/doc repair
paths match the handoff and remain inside the Text Editor boundary or its
focused test/documentation owners.

### Exact configure and focused builds

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Result: exit 0. Release used the identical command with `-B .../release` and
`-DCMAKE_BUILD_TYPE=Release`; exit 0. Both emitted the repository's known
runtime-search-path warnings and completed generation.

```sh
cmake --build <profile-root> --parallel 3 --target \
  qindaqt-editor qindaqt_editor_document_state_tests \
  qindaqt_editor_local_store_tests qindaqt_editor_controller_tests \
  qindaqt_editor_large_document_tests \
  qindaqt_editor_document_collection_tests qindaqt_editor_find_replace_tests \
  qindaqt_editor_restore_state_tests qindaqt_editor_restore_policy_tests \
  qindaqt_editor_window_tests qindaqt_editor_app_shell_tests \
  qindaqt_settings_schema_tests qindaqt_settings_migration_tests
```

Result: Release exit 0, 34/34 repaired incremental actions. Debug exit 0,
67/67 in the final captured rebuild. During review I accidentally started a
second Debug invocation while an earlier 30-second-yielded invocation was still
finishing; Ninja subsequently reported `premature end of file; recovering` for
its generated `.ninja_log`, rebuilt the selected targets, and exited 0. No
source path changed, and every selected Debug binary then passed the full and
focused executions below.

### Debug and Release selectors

For each profile, with `<scratch>` set to a fresh profile-specific directory
beneath the review build root:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  HOME=<scratch>/home XDG_STATE_HOME=<scratch>/state \
  XDG_DATA_HOME=<scratch>/data TMPDIR=<scratch>/tmp \
  ctest --test-dir <profile-root> -R '^qindaqt\.editor-' \
  --output-on-failure --no-tests=error
```

Result: Debug exit 0, 16/16 passed in 2.26 seconds. Release exit 0, 16/16
passed in 1.87 seconds. The counts include the new registry-policy row.

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  HOME=<scratch>/home XDG_STATE_HOME=<scratch>/state \
  XDG_DATA_HOME=<scratch>/data TMPDIR=<scratch>/tmp \
  ctest --test-dir <profile-root> \
  -R '^qindaqt\.settings-(schema|migration)$' \
  --output-on-failure --no-tests=error
```

Result: Debug exit 0, 2/2 passed in 0.04 seconds. Release exit 0, 2/2 passed in
0.03 seconds.

Focused direct QtTest executions used the same isolated environment. In each
profile the selected collection functions emitted 5/5 passes including
init/cleanup; controller 3/3; warning-fatal window 3/3; warning-fatal restore
policy 4/4; and restore-state 3/3. Every command exited 0.

### Scratch controls and ancestor discrimination

```sh
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/repros \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/repros/build \
  -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/repros/build \
  --parallel 3
```

Result: configure exit 0; initial repaired rebuild 6/6 and later fixture
rebuilds 3/3, all exit 0.

```sh
exact_candidate_repros save-as-alias
exact_candidate_repros save-as-chain
exact_candidate_repros save-as-parent-change
exact_candidate_repros opened-parent-change
exact_candidate_repros state-parent-alias
exact_candidate_repros state-ancestor-change
exact_candidate_repros title-boundary
```

Result: each exits 0 with the safe observations recorded above.

I extracted only `a13aa62:src/apps/text_editor` under
`<ROOT>/ancestor-repros/source`, compiled the same controls against those exact
sources, and ran `save-as-alias`, `save-as-chain`, `save-as-parent-change`,
`state-parent-alias`, and `title-boundary`. Each exits 1 by design after
observing the rejected behavior. The ancestor CTest registration check was:

```sh
python3 tests/apps/text_editor/check_registered_environment.py \
  --ctest "$(command -v ctest)" \
  --build-directory /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/dev
```

Result: exit 1 with the missing fatal-warning property. The same checker
against repaired `debug` and `release` exits 0.

### Static gates

```sh
./tools/validate-docs
```

Result: exit 0; 129 Markdown documents and `mkdocs.yml` navigation validated.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/site-r2
```

Result: exit 0; strict documentation built in 1.60 seconds.

```sh
./tools/check-source-shape
```

Result: exit 0; 2,083 source files checked and 0 skipped. It reported the four
existing out-of-lane decomposition-review warnings:
`tst_settings_navigation_page.cpp`, `tests/compositor/CMakeLists.txt`,
`tst_color_model.cpp`, and `tst_audio_applet_controller.cpp`.

```sh
git diff --check
git diff --check a13aa62..d68f8b1
git diff --check b2f5159..d68f8b1
python3 -m json.tool data/settings/schema-v1.json >/dev/null
python3 -m json.tool data/settings/schema-v2.json >/dev/null
```

Result: each exits 0. The two schema files are the only JSON paths changed by
the whole candidate relative to the original base.

### Final immutable-tree check

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse b2f515986150b1acfe82e2807a78a731a58a94a2
git status --porcelain
```

Result: exit 0; all identities remain exact and status remains empty.

## Verdict

The repair closes all four prior findings with non-vacuous registered tests,
ancestor-discriminating controls, and descriptor/canonicalization variants.
The retained external-change, atomic revision-refusal, disabled-restore,
packaging, settings, documentation, and source-boundary evidence remains green.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
