# Rhea Solis — interrupted initial baseline repair midpoint

- **Timestamp:** 2026-08-28T18:14:41Z
- **Base remains:** `4144303f0506e0f33a1ffd29feb952825a9e4d2d`
- **Status:** working; no blocker or collision

The exact cold-start root is repaired in the owned diff. On a successful
watcher replacement, an incomplete first population is known never to have
become LKG, so its provisional item, reverse-identity, last-seen, and degradation
state is discarded before the new epoch directly admits anything. After any
population has completed, the existing staged LKG-preserving path is unchanged.

The atomic helper is present and intentionally decomposed at
`tests/shell/status_notifier/status_notifier_atomic_reconciliation_test_support.h`.
It preserves every prior case and now covers interrupted initial identity
handover; interrupted 64-to-64 capacity replacement with the new key first and
last; Loading before completion; stale prior-epoch registration/completion;
completed same-owner and cross-owner handover; completed capacity replacement
both orders; conflict rollback and next-epoch recovery both orders; malformed
new input; and invalid-capacity rollback plus recovery.

Fresh current evidence: strict incremental focused build 9/9 actions; exactly
three StatusNotifier CTest rows 3/3; complete registry QtTest 25/25; the two
public atomic rows 4/4; source shape 1019/0 with every helper below the function
threshold; docs 65/navigation; and whitespace. I am proceeding through a fresh
strict build directory, full direct and adjacent gates, strict MkDocs, exact
provenance, one non-amended commit, and clean handoff to retained reviewer
Shannon the 2nd. No help is currently needed; this exact interrupted-baseline
fixture is available to Shell peers if useful.
