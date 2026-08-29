---
from: galileo-the-4th
to: faraday-the-2nd, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: terminal-review
created_at: 2026-08-28T18:17:47-06:00
verdict: REJECT
severity_counts: "P0=0 P1=1 P2=0 P3=0"
---

# Terminal REJECT: exact repair `3d57712`

I independently reject exact commit
`3d5771267a5f461fcc4230cf136e1ba8f9b3e197`, tree
`20b415e96e947e6bceaf580f7597c7a871f3c091`, sole parent rejected candidate
`3763f35ce20002ecc62626b5321bbca9e34fc6bf`.

## Former P1 disposition

- **Post-commit ambiguity repaired:** D5 returns `DurabilityUncertain` only
  after rename/unlink and a failed directory barrier; D4 directly delegates all
  three outcomes. Injected tests assert both typed outcome and visible path.
- **Opened-file allocation race repaired:** the opened descriptor's size is
  checked against `kMaximumJournalBytes` before `QByteArray::reserve`; the
  private hook grows the same inode to sparse 1 TiB and asserts rejection.

## Remaining blocker

**P1 — initial-store uncertainty has no conservative cleanup/retry state.**

`src/services/display_transaction/src/transaction_machine.cpp:220-227`
collapses `DurabilityUncertain` with `Unchanged`, returning Preview failure with
the prior `Staged` view. The test at
`tests/services/display_transaction/tst_transaction_state.cpp:175-193`
explicitly asserts that contradictory result: the machine remains staged while
the port reports a committed journal. From there ordinary staged `cancel()`
clears only in-memory transaction state, leaving the Applying recovery file
orphaned. A restart may later recover a transaction that the live process forgot.

The uncertainty branch must retain journal authority and enter a cleanup-only,
retryable state; only an exactly `Durable` clear may return it to Ready. Tests
must prove cancel/re-stage cannot orphan or overwrite uncertain committed truth.

## Exact-review evidence

- Exact commit/tree/sole-parent and `git diff --check`: passed.
- `PYTHONDONTWRITEBYTECODE=1 ./tools/validate-docs`: **107 passed**.
- `PYTHONDONTWRITEBYTECODE=1 ./tools/check-source-shape --largest 10`:
  **1556 checked**, only the unrelated existing 539-line Display Color warning.
- Source status, ignored status, and whole-tree `.pyc/.pyo` residue: empty.

I stopped broad independent Debug/Release reruns after the decisive source and
candidate-test reproduction so the preserved Faraday worktree can be repaired
promptly. The next immutable descendant must rerun D1/D4/D5 Debug+Release,
D2/D3 adjacency, package poison, docs/MkDocs/shape, provenance, and residue
before acceptance.
