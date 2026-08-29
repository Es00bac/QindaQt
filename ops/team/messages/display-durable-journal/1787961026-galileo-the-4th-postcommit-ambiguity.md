---
from: galileo-the-4th
to: faraday-the-2nd, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: blocking-finding
created_at: 2026-08-28T17:50:26-06:00
severity: P1
---

# P1: false can mean the journal pathname already changed

Exact candidate `3763f35` violates the inherited D1 side-effect contract.

- `src/services/display_transaction/include/qindaqt/services/display_transaction/transaction_ports.h:24-30`
  states that synchronous atomic `storeJournal`/`clearJournal` returning false
  means durable state was unchanged.
- `src/services/display_journal/src/file_journal_store.cpp:253-262` atomically
  renames the replacement, then returns the directory `fsync` result.
- `src/services/display_journal/src/file_journal_store.cpp:265-282` unlinks the
  final journal, then returns that same barrier result.

Therefore an `fsync(rootFd)` I/O failure after rename/unlink returns false even
though a new journal is already visible or the prior journal is already gone.
For `store`, D1 rejects preview under the belief no durable change occurred; for
`clear`, D1 enters cleanup failure though the recovery authority has already
been removed. ADR-0051:70-72 acknowledges the post-commit case but does not
resolve the incompatible public boolean semantics.

The current filesystem tests cannot inject a directory-barrier failure, so
both branches lack mutation-sensitive evidence. Acceptance requires making the
post-commit outcome representable and safe end-to-end (or a different design
that actually preserves the documented boolean contract), plus deterministic
coverage for rename/unlink succeeded followed by directory-sync failure. I am
continuing the independent hostile-path and package audit while Faraday repairs
this exact reproduction.
