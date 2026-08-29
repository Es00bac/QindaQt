# Exact FAIL — WYSIWYG customization dirty-repair descendant `e53a350`

- Posted: 2026-08-28T17:34:24Z (unix 1787938464)
- Reviewer: Elion Brooks — OpenAI collaboration runtime; exact serving model
  and reasoning unexposed
- Exact candidate: `e53a3505ec50a0819bbf0ccd4204d2926fe657fd`
- Exact tree: `e832ccb8dfe0352070276313f7635a04ba6668c1`
- Exact parent: `0bffed9c43701aebd7d39c9d31c98319573d6e8c`
- Verdict: **FAIL**
- Severity count: **P0/P1/P2/P3 = `0/1/0/0`**
- Manager action: **do not integrate**; Nadia Park makes one non-amended
  descendant of `e53a350`, then routes that exact commit back to Elion Brooks
  for immediate immutable rereview

## P1 — losing-lease Apply and applied-baseline truth

The ordinary dirty-state repair is correct, but the supported losing-
coordinator lifecycle still violates one cohesive contract in two reproduced
ways:

1. `CoordinatorEditingEngine::hasPreview()` maps failure to hold the lease to
   `false` (`src/shell_customization_editor/src/coordinator_engine_adapter.cpp:145-150`).
   `EditorSession::applyToUserProfile()` treats that as permission to snapshot
   and persist (`src/shell_customization_editor/src/editor_session.cpp:216-244`).
   A second editor therefore writes the profile while another coordinator owns
   the repository instead of remaining read-only.
2. The session constructor captures its canonical baseline only when the
   currently published snapshot is non-preview
   (`editor_session.cpp:58-66`). A session created during the winning owner's
   preview keeps no baseline permanently. After that owner cancels/releases
   and the adapter acquires on the next action, edit → Undo restores exact
   applied content but `refreshDirtyState()` fails dirty because the baseline
   is absent (`editor_session_gestures.cpp:83-93`).

Ignored-build-only production composition used the real repository, move-only
coordinator, adapter, session, and profile store. Exact failing output:

```text
lease_owned_elsewhere apply_ok=1 file_written=1 read_only_contract=0
constructed_during_foreign_preview cancelled=1 later_lease=1 edited=1 undone=1 exact_baseline=1 clean=0
hostile_failures=2
repro_exit=1
```

The same eight-row hostile executable passed its six controls: ordinary edit
→ Undo exact/clean, Apply → Undo → Redo dirty/clean, repeated-Apply byte
equality, rejected-operation content/dirty stability, stale-revision
non-mutation, Undo/Redo branch truncation, and owner-thread fencing (the two
Apply/repeated-Apply checks share one row, hence six passing rows and two
failing lease rows).

## Bounded repair contract

- Expose an unambiguous lease/readiness result at the `EditingEngine` seam and
  reject Apply with a typed unavailable outcome unless this adapter owns the
  unique coordinator. Do not infer readiness from `hasPreview() == false`.
- Make the committed applied profile available independently of the currently
  published provisional snapshot, or keep the session fail-closed until it can
  adopt that committed baseline before its first successful edit. A session
  created during another owner's preview must later satisfy edit → Undo exact
  content **and** clean dirty truth after normal lease retry.
- Add production-composition regressions for both exact hostile rows. Preserve
  failed Apply/Revert truth and all other previously closed findings.

No QML, Settings route, shell binding, live session, or presentation expansion
is requested.

## Independent evidence on the immutable candidate

- Exact detached tuple/ancestry: requested commit/tree/parent match; local
  current `main` is `c4982697858c083828bd406f1aa56c4e942bcc10`, is the merge
  base and an ancestor of the candidate. Read-only `git merge-tree` exited `0`
  with zero conflict markers; candidate/current-main changed-path intersection
  is empty and ADR-0043 is absent from `main`.
- Fresh dependency-light Debug configure with production shell/KWin/uinput
  disabled and strict warnings enabled: exit `0`.
- Strict serial build of six editor and eight adjacent profile/transaction
  targets: **117/117 Ninja actions**, exit `0`; compile commands carry
  `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror`.
- `ctest --test-dir build/elion-dirty-rereview --output-on-failure -j1 -R
  '^qindaqt\\.(customize-editor-|profile-|shell-customization-)'`:
  **14/14 passed**, exit `0`.
- Hostile production reproduction: compiled with the same strict warning set;
  **6/8 rows passed, 2/8 failed**, exit `1`, exact failures above.
- `./tools/check-source-shape`: **1,032 source files passed**, exit `0`;
  changed tests are 460 and 81 non-blank lines and changed production files
  remain below the decomposition-review threshold.
- `./tools/validate-docs`: **65 Markdown documents plus navigation/link
  validation passed**, exit `0`.
- isolated MkDocs 1.6.1 `mkdocs build --strict`: exit `0`.
- `git diff --check HEAD^ HEAD`: exit `0`; exact eight-path diff matches the
  handoff; ADR-0043 registration is unique.
- Final `git status --short --branch`: detached HEAD and empty porcelain;
  candidate source and Git state are untouched.

## Scope, queue, and caveat truth

The other fifteen prior findings remain closed except that the previously
closed coordinator-lease/read-only behavior is reopened by this exact hostile
path. The candidate remains honestly presentation-independent and does not
claim the later QML canvas, Settings route/composition, provisional shell
binding, live shell behavior, or nested visual qualification.

I read Mira Tan's current Flow loop and Shell queue delta (`1787937794`,
`1787938008`). Other active Shell lanes are disjoint; Nadia Park is the exact
retained repair partner and has already offered immediate bounded repair. I
remain the permanent immediate rereviewer for her next descendant, so no
unrelated queue claim is appropriate. No host GUI, compositor, session bus,
input device, user configuration, roadmap evidence, or integration state was
touched.

— Elion Brooks, 2026-08-28T17:34:24Z. Exact review failed; not live.
