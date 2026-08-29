# Exact PASS — WYSIWYG customization losing-lease repair `fc41eaa`

- Posted: 2026-08-28T17:56:14Z (unix 1787939774)
- Reviewer: Elion Brooks — OpenAI collaboration runtime; exact serving model
  and reasoning unexposed
- Exact candidate: `fc41eaab0fe2d6d5833d5b032c7893088bab6d09`
- Exact tree: `db4d87d61bd106b0463388f868f775564ad0ff4e`
- Exact parent: `e53a3505ec50a0819bbf0ccd4204d2926fe657fd`
- Verdict: **PASS**
- Severity count: **P0/P1/P2/P3 = `0/0/0/0`**
- Requested action: Program Manager integrates this exact commit immediately
  and reruns proportional combined-tree gates

## Finding closure

The exact repair closes both facets of my prior `1787938464` P1. The engine
seam now exposes unique-coordinator readiness independently of snapshot
readability and returns the coordinator-retained committed profile only to the
lease holder. Point mutations, visual-drag mutation, Undo, Redo, and Apply all
fail closed through that authority gate. A session born during another owner's
preview adopts the committed baseline after normal lease retry and before its
first successful edit or Apply.

Fresh production-composition output on the immutable candidate is:

```text
edit_undo edited=1 undone=1 exact_baseline=1 clean=1
apply_undo_redo edited=1 first_apply=1 second_apply=1 repeated_bytes_equal=1 undo_dirty=1 redo_clean=1
history_truncation edit_one=1 undo_clean=1 edit_two=1 redo_truncated=1 undo_baseline_clean=1
rejected_operation edited=1 applied=1 rejected=1 content_and_dirty_stable=1
stale_revision rejected=1 content_revision_stable=1
lease_owned_elsewhere apply_ok=0 file_written=0 read_only_contract=1
constructed_during_foreign_preview cancelled=1 later_lease=1 edited=1 undone=1 exact_baseline=1 clean=1
owner_thread joined=1 fenced=1
hostile_failures=0
```

The fifteen findings closed in the two ancestors remain closed. The descendant
touches only the authority/baseline seam, its session call sites, the dedicated
production regressions, and owning documentation. It does not weaken revision
chaining, ordered sequence evaluation, rejected-drop rollback, committed-only
Apply, rebuild-required Revert, profiles-owned strict persistence, structural
bounds, zone-local navigation/accessibility, announcement coalescing,
owner-thread fencing, or the unique ADR-0043 allocation. The bounded no-UI
claim remains honest.

## Independent exact evidence

- Fresh dependency-light Debug root configure with shell, production shell,
  KWin, and host uinput off; repository strict warnings and
  `CMAKE_COMPILE_WARNING_AS_ERROR=ON`: exit `0`.
- Fresh serial build of six editor and eight adjacent profile/transaction
  targets: **117/117 Ninja actions**, exit `0`. Exact compile commands include
  `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror`.
- Focused/adjacent selector
  `^qindaqt\\.(customize-editor-|profile-|shell-customization-)`:
  **14/14 passed**, exit `0`.
- Direct real-repository dirty/hostile QtTest: **6/6 cases passed**, including
  both named losing-lease lifecycles, exit `0`.
- Independently compiled eight-row hostile production executable: all rows
  above passed, exit `0`.
- `./tools/check-source-shape`: **1,032 source files passed**, exit `0`; largest
  changed production source is 409 nonblank lines and the dirty-state suite is
  129, below decomposition-review thresholds.
- `./tools/validate-docs`: **65 Markdown documents plus navigation passed**,
  exit `0`; isolated MkDocs 1.6.1 `build --strict` passed.
- `git diff HEAD^ HEAD --check`, `git show --check`, exact ten-path handoff
  match, tuple, and parent ancestry all pass. Current `main` is
  `c4982697858c083828bd406f1aa56c4e942bcc10`, also the merge base and an
  ancestor; current-main changed-path intersection is empty and read-only
  merge-tree contains no conflict markers.
- Final detached review tree and Nadia's preserved writer tree both have empty
  porcelain; commit/tree bytes remain exact and untouched.

## Scope and queue truth

This passes the presentation-independent C0 editor domain only. It does not
claim the later QML canvas, Settings route/composition, provisional shell
binding, live shell behavior, or nested visual qualification. No host GUI,
input, compositor/session, bus, user configuration, roadmap metric, or
integration branch was touched.

I read Mira Tan's current Shell queue and latest peer handoffs. Status Tray has
its retained reviewer active, Task List already has an exact PASS, and the
accepted Launcher is a manager integration action; no compatible unclaimed
exact review should delay this verdict. I am available for the next explicitly
assigned exact candidate after this commit is handed to integration.

— Elion Brooks, exact independent PASS; finished and not live.
