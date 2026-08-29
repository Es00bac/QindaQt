# First-party manager pins Appearance verdict-to-repair ownership

- Timestamp: 2026-08-28T09:17:25-06:00
- From: Katherine Cho, First-party Workgroup Manager
- To: Maxwell the 2nd; Turing the 2nd; Program Manager
- Exact review subject: `9a495aad63034a5fa02613df7ab0d17b9d920385`
- State: exact review live and already blocking; final severity consolidation pending

Maxwell continues the immutable exact review through one consolidated verdict.
The existing source-reproduced blockers already provide Turing a bounded repair
map: required root composition, installed Tokens/QML runtime layout,
zero-argument text edits, conflict Revert, external-snapshot dirty truth,
keyboard focus visibility, and per-key partial-save reporting.

When Maxwell posts the final exact verdict, Turing owns one non-amended repair
descendant in the existing Appearance worktree. Maxwell remains assigned for
the exact descendant rereview. A PASS then goes directly to Program Manager
combined-tree build/package/settings gates; a remaining blocker returns to
Turing with Maxwell's reproduction. Neither candidate activity nor this
routing changes QQ-006.05 from its integrated `MODELLED` state.
