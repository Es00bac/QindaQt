# Olga Ladyzhenskaya — independent application review

- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `a13aa620f7ad1a4756b8a5e2ab8ae7887244d0b3`
- Tree SHA: `21258eaa5332feb22ddb0101136dd4b75f7618ef`
- Parent SHA: `b2f515986150b1acfe82e2807a78a731a58a94a2`
- Base SHA: `b2f515986150b1acfe82e2807a78a731a58a94a2`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/text-editor-s2-codex-review`
- Review build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex`

The candidate and base identities matched before review. `git status --porcelain`
was empty before review and after all build, test, static-analysis, and scratch
reproduction work. I did not edit, commit, amend, rebase, or otherwise mutate a
product path. I did not run `tests/session`, a compositor, host D-Bus services,
hardware, uinput, or network access.

## Findings ledger

### P0

None.

### P1

#### P1-1 — Save As through a symlinked parent breaks canonical-path uniqueness

`DocumentController::normalizePath()` at
`src/apps/text_editor/document/document_controller.cpp:28` falls back to the
lexical absolute path when the destination does not exist. `DocumentCollection::saveAs()`
at `src/apps/text_editor/document/document_collection.cpp:83` records that value
without re-canonicalizing after the successful create. Once the file exists,
opening its real path canonicalizes to a different string, so the collection
admits a second controller for the same canonical file. This violates the S2
collection invariant in ADR-0064 and the owning wiki, and gives one file two
independent byte revisions, watchers, dirty states, and save authorities.

Reproduction source is confined to
`/home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/repros/main.cpp`.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/repros/build/exact_candidate_repros save-as-alias
```

Result: exit 1 (the probe deliberately fails when it observes the defect).

```text
save-ok=1
stored-path=.../alias/shared.txt
real-canonical=.../real/shared.txt
reopen-ok=1
focused-existing=0
document-count=2
expected-focused-existing=1
expected-document-count=1
```

Expected: the second admission focuses the existing controller and the count
remains one. Observed: both operations succeed and the count becomes two.
The registered `qindaqt.editor-multiple-documents` row still passes because
`tests/apps/text_editor/tst_document_collection.cpp:69` checks only an already
existing direct-path collision, not a newly created destination beneath a
symlinked parent.

#### P1-2 — Restore persistence follows a symlinked ancestor outside its state root

`RestoreStateStore::store()` at
`src/apps/text_editor/restore/restore_state_store.cpp:145` checks only whether
the final `m_stateDirectory` entry itself is a symlink. When an ancestor such as
`$XDG_STATE_HOME/qindaqt` is a symlink and the final `text-editor` directory does
not yet exist, `QDir::mkpath()` follows that ancestor and `QSaveFile` writes the
inventory outside `$XDG_STATE_HOME`. This violates the explicit fail-closed
symlink-escape and state-root containment contracts.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/repros/build/exact_candidate_repros state-parent-alias
```

Result: exit 1.

```text
store-ok=1
store-error=0
escaped-file-exists=1
escaped-file=.../escaped/text-editor/open-documents-v1.json
expected-store-ok=0
expected-escaped-file-exists=0
```

Expected: reject the symlink-escaping root with `InvalidRoot` and create no
file. Observed: the store reports success and creates the inventory in the
symlink target. The registered restore-state test checks only a symlink in the
final inventory-file position, so all focused tests remain green.

### P2

#### P2-1 — Whitespace collapse can produce a 129-unit tab title

At `src/apps/text_editor/ui/document_title.cpp:37`, a pending collapsed space is
appended before the next ordinary character, but the maximum is checked only
after that character is appended. A 127-unit prefix followed by whitespace and
one character therefore exceeds the documented 128 UTF-16-unit limit.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/repros/build/exact_candidate_repros title-boundary
```

Result: exit 1.

```text
title-size=129
documented-maximum=128
valid-utf16=1
```

Expected: size at most 128. Observed: size 129. The current title test uses a
long uninterrupted suffix and does not exercise whitespace at the boundary.

#### P2-2 — Three offscreen/package rows do not register fatal Qt warnings

`tests/apps/text_editor/CMakeLists.txt:149`, `:159`, and `:179` register the
multi-path CLI, hostile-argv CLI, and installed-runtime rows without
`QT_FATAL_WARNINGS=1`, although they launch Qt with the offscreen platform and
the review contract requires offscreen rows to carry that negative control.
The four rows explicitly named `*-offscreen` do register it.

Reproduction:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/dev --show-only=json-v1 |
  python3 -c '<print the ENVIRONMENT property for the seven Qt/offscreen editor rows>'
```

Result: exit 0. The four `*-offscreen` rows include
`QT_FATAL_WARNINGS=1`; `qindaqt.editor-cli-multiple-paths` and
`qindaqt.editor-cli-hostile-argv` list `QT_QPA_PLATFORM=offscreen` without the
fatal-warning variable, while `qindaqt.editor-installed-theme-and-metadata`
also lacks it (its child scripts set the offscreen platform themselves).

For distinction, externally injecting the missing condition currently passes:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/dev \
  -R '^qindaqt\.editor-(cli-multiple-paths|cli-hostile-argv|installed-theme-and-metadata)$' \
  --output-on-failure --no-tests=error
```

Result: exit 0, 3/3 passed. Release: exit 0, 3/3 passed. Thus this is a missing
registered negative control, not a current Qt warning.

### P3

None.

## Review-question evidence

1. **Per-document guarantees:** independent controllers, views, undo stacks,
   external state, and active-index save routing are present. The focused rows
   pass, the controller continues to pass its own retained-byte-revision into
   `SavePolicy::MatchRevision`, and close-window planning is recomputed after
   the modal choice so a document saved during the prompt is no longer dirty.
   However, P1-1 disproves the required canonical uniqueness after Save As, so
   the complete guarantee does not hold.
2. **Find/replace:** the 256-unit pattern admission rejects quantifiers,
   grouping, alternation, lookaround, numeric backreferences, malformed
   classes, and the classic `(a+)+$` case before scanning; results are capped at
   10,000. The executed tests prove case/whole-word/wrap/no-match behavior, the
   hostile rejection under 100 ms, and one-Undo Replace All. Actions expose the
   documented shortcuts, and textual status is emitted through polite
   accessibility announcements. I found no additional P0-P2 find/replace
   product defect.
3. **Restore policy:** both schema files append the Boolean key with default
   `false`; the Settings1 fake-transport tests prove baseline loss, mandatory
   fresh confirmation, conflict handling, and uncertain no-replay. The JSON is
   exact and paths-only, and unavailable targets are dropped with a bounded
   notice. P1-2 nevertheless disproves the required symlink-escape rejection
   and physical containment beneath `$XDG_STATE_HOME`.
4. **Tests and shape:** all 15 focused rows and both settings rows pass in Debug
   and Release with home/state/data/temp paths under the build root and ambient
   buses/displays disconnected. The hostile argv control, source-policy poison,
   additive schema diffs, JSON validation, docs, strict MkDocs, source-shape,
   and diff gates pass. The candidate keeps portals, print, extensions, rich
   text, nested screenshots, and whole-application AT proof excluded. The
   green suite misses all three product reproductions above, and P2-2 records
   the additional fatal-warning registration gap.

## Commands and results

### Identity and diff inspection

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse b2f5159
git status --porcelain
git diff --name-status b2f5159..a13aa620f7ad1a4756b8a5e2ab8ae7887244d0b3
git diff --stat b2f5159..a13aa620f7ad1a4756b8a5e2ab8ae7887244d0b3
```

Result: all commands exit 0; candidate/tree/parent/base match the header; status
is empty; 49 changed paths are within owned paths or the declared additive
coordination points. The schema diffs append exactly one key each.

### Configure and focused builds

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/dev -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Result: exit 0. Release used the identical command with `-B .../release` and
`-DCMAKE_BUILD_TYPE=Release`; exit 0. Configure emitted the repository's Qt/KF
runtime search-path warnings and generated-qmldir notices but completed.

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

Result: Debug exit 0, 225/225 build steps. Release exit 0, 225/225 build steps.

### Debug and Release selectors

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/dev \
  -R '^qindaqt\.editor-' --output-on-failure --no-tests=error
```

Result: exit 0, 15/15 passed. Release equivalent: exit 0, 15/15 passed.

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/dev \
  -R '^qindaqt\.settings-(schema|migration)$' --output-on-failure --no-tests=error
```

Result: exit 0, 2/2 passed. Release equivalent: exit 0, 2/2 passed.

### Static gates

```sh
./tools/validate-docs
```

Result: exit 0; 129 Markdown documents and navigation validated.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/site
```

Result: exit 0.

```sh
./tools/check-source-shape
```

Result: exit 0; 2,082 files checked, 0 skipped. It reported four pre-existing
500-line decomposition-review warnings outside this lane:
`tst_settings_navigation_page.cpp`, `tests/compositor/CMakeLists.txt`,
`tst_color_model.cpp`, and `tst_audio_applet_controller.cpp`.

```sh
git diff --check b2f5159..a13aa620f7ad1a4756b8a5e2ab8ae7887244d0b3
git diff --check
python3 -m json.tool data/settings/schema-v1.json >/dev/null
python3 -m json.tool data/settings/schema-v2.json >/dev/null
```

Result: each exit 0.

### Scratch reproduction build

```sh
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/repros \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/repros/build \
  -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-editor-s2-codex/repros/build \
  --parallel 3
```

Result: configure exit 0; build exit 0, 10/10 steps. All runtime files and
symlink targets used by the probes were created only beneath the assigned
review build root.

### Final immutable-tree check

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse b2f5159
git status --porcelain
```

Result: exit 0; identities remain exact and status remains empty.

## Verdict

The candidate violates canonical single-owner document identity and permits its
restore writer to escape the selected state root through an ancestor symlink.
Both are blocking S2 contract failures. The title bound and fatal-warning test
registration are additional P2 defects.

VERDICT REJECT P0/P1/P2/P3=0/2/2/0
