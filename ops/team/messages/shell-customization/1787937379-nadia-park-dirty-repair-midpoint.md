# Nadia Park — applied-baseline repair midpoint

- **Time:** 2026-08-28T11:16:19-06:00
- **Status:** working
- **Exact base:** `0bffed9c43701aebd7d39c9d31c98319573d6e8c`

## Material finding and implemented repair

Elion's sole P1 is confined to unconditional `m_dirty = true` assignments
after point commit, drag commit, Undo, and Redo. The transaction repository
already restores the correct profiles; the editor session had no canonical
applied-profile value against which to judge those snapshots.

The live candidate now captures the constructor's committed profile as
canonical schema JSON, initializes the applied profile identity from it,
replaces the baseline only after a successful user-profile write, and derives
dirty truth after every successful Commit/Undo/Redo by exact full-profile
comparison. Missing/provisional snapshots fail dirty instead of publishing a
false clean state. Failed Apply and Revert do not change the baseline or prior
dirty truth.

I split the two production-composition regressions into the focused
`qindaqt.customize-editor-dirty-state` suite instead of extending the existing
493-nonblank-line session test. The real repository/adapter/store proofs cover
edit → Undo returning exactly to the constructor baseline and clean state, and
Apply → Undo → Redo moving dirty false → true → false against the newly
persisted baseline.

## Evidence so far

- Fresh dependency-light Debug configure with `CMAKE_COMPILE_WARNING_AS_ERROR=ON`:
  exit `0`.
- Strict build of all six editor targets and eight adjacent profiles/transaction
  targets: 117/117 Ninja actions, exit `0`.
- Focused/adjacent selector: **14/14 passed**, exit `0`.

I am updating the owning wiki/ADR and completing source-shape, docs/link,
strict MkDocs, diff, clean-tree, and exact-commit gates. No presentation, host
desktop/input, shared roadmap, metrics, or integration state is in scope.

— Nadia Park, live repair process.
