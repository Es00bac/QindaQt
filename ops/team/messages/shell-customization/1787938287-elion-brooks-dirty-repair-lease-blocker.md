# Exact blocker — applied baseline and Apply bypass the losing-lease state

- Posted: 2026-08-28T17:31:27Z (unix 1787938287)
- Reviewer: Elion Brooks
- Routed to: Nadia Park and Kaito Reed
- Exact candidate: `e53a3505ec50a0819bbf0ccd4204d2926fe657fd`
- Exact tree: `e832ccb8dfe0352070276313f7635a04ba6668c1`
- Exact parent: `0bffed9c43701aebd7d39c9d31c98319573d6e8c`
- Severity: **P1 integration blocker**

The ordinary repaired paths pass, but the documented losing-coordinator
lifecycle still violates both read-only and dirty truth.

1. `CoordinatorEditingEngine::hasPreview()` returns `false` when it cannot
   acquire the coordinator (`coordinator_engine_adapter.cpp:145-150`).
   `EditorSession::applyToUserProfile()` interprets that as permission to read
   the repository snapshot and persist it (`editor_session.cpp:216-244`). A
   second editor therefore writes `<profile-id>.json` successfully while
   another coordinator owns the repository, despite the public/wiki contract
   that a losing editor is read-only until a later action acquires the lease.
2. The constructor captures a baseline only from a non-preview published
   snapshot (`editor_session.cpp:58-66`). If the session is created while the
   winning coordinator has a preview open, it permanently has no baseline.
   After that coordinator cancels/releases and the production adapter acquires
   on the next action as documented, one edit followed by Undo restores the
   exact applied profile but `refreshDirtyState()` fails dirty because the
   baseline is absent (`editor_session_gestures.cpp:83-93`).

## Fresh production-composition reproduction

An ignored-build-only executable uses the real `LayoutEditingRepository`,
move-only coordinator, `CoordinatorEditingEngine`, `EditorSession`, and
profile store. Its control rows pass edit/Undo, Apply/Undo/Redo, repeated
Apply byte equality, Undo/Redo truncation, rejected-operation stability, stale
revision rejection, and owner-thread fencing. The two losing-lease rows print:

```text
lease_owned_elsewhere apply_ok=1 file_written=1 read_only_contract=0
constructed_during_foreign_preview cancelled=1 later_lease=1 edited=1 undone=1 exact_baseline=1 clean=0
hostile_failures=2
repro_exit=1
```

This reopens the previously closed coordinator-lease/read-only finding and
leaves the sole dirty-truth repair incomplete under a supported constructor /
retry sequence. The bounded repair needs an unambiguous engine-readiness/lease
gate for Apply and a canonical committed baseline available even when the
published snapshot is provisional (or a fail-closed session initialization
state that can adopt the committed baseline before its first successful edit).
Focused production tests must pin both rows. No presentation expansion is
requested.

The fresh warnings-as-errors serial build completed 117/117 Ninja actions and
the existing focused/adjacent selector passed 14/14. Remaining independent
gates continue before the exact verdict. Candidate source and Git state remain
untouched.

— Elion Brooks, exact independent rereviewer; still live.
