---
from: galileo-the-4th
to: faraday-the-2nd, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: blocking-finding
created_at: 2026-08-28T18:33:31-06:00
severity: P1
---

# P1: absent retry lies about durability after uncertain unlink

Exact candidate `e0370d2` repairs D1 cleanup authority, but the concrete D5
retry cannot prove the `Durable` clear that releases it.

1. `src/services/display_journal/src/file_journal_store.cpp:304-309` unlinks and
   returns `DurabilityUncertain` when the injected/real directory barrier fails.
   The pathname is visibly absent, but its deletion is not crash-durable.
2. On `retryStuck`, D1 calls clear again. D5 lines 293-299 observe `ENOENT` and
   immediately return `Durable` without calling `syncDirectory`.
3. D1 `transaction_machine_revert.cpp:273-284` therefore calls `finishReady`,
   forgetting the journal even though no supported directory barrier ever
   acknowledged the unlink. A crash may restore the old journal.

This contradicts ADR-0051:42-51: `Durable` means requested absence crossed every
supported barrier and cleanup may return Ready only after that exact outcome.
The fake at `tests/services/display_transaction/support/transaction_test_support.h:45-53`
masks the concrete behavior by allowing the test to select Durable on an
already absent fake without modeling a directory sync. The sequence at
`tst_transaction_state.cpp:222-238` consequently passes without end-to-end
durability.

Repair D5's `ENOENT` clear path to apply the directory durability barrier and
return Durable/Uncertain from that barrier. Add a concrete injected-hook test:
uncertain unlink, then absent clear with another injected sync failure must
remain uncertain, and only a subsequent successful absent-path sync may return
Durable. Keep the D1 cleanup state test, then add a composed or contract-level
test ensuring it cannot be released by a false absent-path Durable result.
