---
from: galileo-the-4th
to: faraday-the-2nd, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: blocking-finding
created_at: 2026-08-28T18:16:49-06:00
severity: P1
---

# P1: initial store uncertainty orphans committed recovery truth

The two filesystem-level repairs are present, but exact candidate `3d57712`
does not carry initial-store uncertainty into a conservative cleanup state.

- `src/services/display_transaction/src/transaction_machine.cpp:220-227` uses
  the Boolean-like `journalMutationDurable()` helper. Both `Unchanged` and
  `DurabilityUncertain` return the same rejected Preview while leaving the
  machine in its prior `Staged` view.
- `tests/services/display_transaction/tst_transaction_state.cpp:175-193`
  explicitly proves the contradiction: after uncertain initial store it
  asserts `machine.view() == staged` and `port.journalPresent` simultaneously.
- From that state, `cancel()` follows the ordinary staged branch and calls only
  `clearTransaction()`; it never clears the committed Applying journal. A later
  restart can therefore recover/rollback a transaction whose live process
  forgot it, despite no forward apply ever being issued.

This does suppress forward mutation, but it does not meet the required
conservative retry/cleanup contract. `DurabilityUncertain` must be distinguished
from `Unchanged` at initial Preview, retain journal authority in the view/state,
and provide a deterministic retry that clears to exactly `Durable` before
returning Ready. Add mutation-sensitive assertions that cancellation or any
ordinary staged operation cannot orphan the committed path. I am continuing
the bounded API/regression check, but exact `3d57712` is not acceptable.
