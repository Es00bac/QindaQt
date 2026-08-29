# Exact blocker — Undo/Redo lose applied-profile dirty truth in `0bffed9c`

- Posted: 2026-08-28T17:07:25Z (unix 1787936845)
- Reviewer: Elion Brooks
- Routed to: Nadia Park and Kaito Reed
- Exact candidate: `0bffed9c43701aebd7d39c9d31c98319573d6e8c`
- Exact tree: `75bed4c52faa41694a5c76d806a1bfa7a63780ee`
- Exact parent: `42200c8f3a8f24deffe69ccec26737d796dc09ad`
- Severity: **P1 integration blocker**

`EditorSession` claims ownership of dirty/applied state, but it retains neither
the constructor-time applied profile nor the last successfully applied profile
value. Successful Undo and Redo therefore assign `m_dirty = true`
unconditionally (`src/shell_customization_editor/src/editor_session.cpp:160-208`).
The ordinary sequence edit → Undo can restore the exact applied profile while
the public session still reports unsaved changes. The same defect occurs after
Apply → Undo → Redo returns to the newly applied profile. It causes a false
dirty close prompt and makes Revert/Apply presentation state untruthful.

## Minimal production reproduction

I compiled an ignored-build-only program against the exact candidate's real
`LayoutEditingRepository`, `CoordinatorEditingEngine`, and `EditorSession`:

1. Capture the initial applied `LayoutProfile`.
2. Commit one cross-panel applet drag through the production adapter.
3. Call `session.undo()`.
4. Compare repository JSON with the captured applied profile and query
   `session.isDirty()`.

Observed exact output:

```text
exact_baseline=1 dirty=1
repro_exit=1
```

The existing production undo test proves profile restoration but stops at
repository history (`tests/shell_customization_editor/tst_editor_session.cpp:301-336`);
it never asserts session dirty truth.

## Required bounded repair

Retain a canonical applied-profile baseline in the session, initialize it from
the committed constructor snapshot, replace it only after a successful Apply,
and derive dirty truth after commit/Undo/Redo by exact canonical profile
comparison. Add production tests for edit → Undo returning clean and Apply →
Undo → Redo returning clean; keep failed Apply and Revert behavior unchanged.
No presentation, Settings1, or GUI expansion is requested.

All eight former P1 findings are otherwise closed, and the fresh build plus
13/13 focused/adjacent tests pass. Candidate source was not modified.

— Elion Brooks, exact independent reviewer.
