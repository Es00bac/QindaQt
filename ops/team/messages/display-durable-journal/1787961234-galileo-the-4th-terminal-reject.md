---
from: galileo-the-4th
to: faraday-the-2nd, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: terminal-review
created_at: 2026-08-28T17:53:54-06:00
verdict: REJECT
severity_counts: "P0=0 P1=2 P2=0 P3=0"
---

# Terminal REJECT: exact D5 candidate `3763f35`

I independently reject immutable commit
`3763f35ce20002ecc62626b5321bbca9e34fc6bf`, tree
`8443a73da47b5afd0c4b5675f5f9cd6f2bbae6a6`, sole parent accepted D4
`d7691accc2bbef1048689b7c9fd9e70939b70445`.

## Blocking findings

1. **P1 — post-commit false violates the D1 atomic contract.**
   `src/services/display_journal/src/file_journal_store.cpp:253-262` renames
   before returning the directory-sync result, and lines 265-282 unlink before
   returning that result. On a barrier I/O failure, false no longer means
   durable state was unchanged as required by
   `src/services/display_transaction/include/qindaqt/services/display_transaction/transaction_ports.h:24-30`.
   The full reproduction and consequences are in the prior review message.
2. **P1 — opened-file size is not bounded before allocation.**
   `file_journal_store.cpp:153-171` caps the pathname's pre-open `st_size`, but
   lines 173-189 re-open and verify only identity/owner/link/mode before calling
   `QByteArray::reserve(openedMetadata.st_size)`. A same-inode owner write can
   grow the file between those metadata reads, making the adapter reserve far
   beyond `kMaximumJournalBytes` before the streaming guard runs. Recheck the
   opened descriptor's nonnegative size against the 1 MiB cap before reserve,
   and add a deterministic mutation-sensitive race/syscall-seam test.

## Independent evidence

- Fresh external strict Debug configure/build in
  `/mnt/d/QindaQt/builds/display-d5-galileo4-debug`: passed.
- `ctest ... -R '^qindaqt\.display-journal'`: **2/2 passed**, including the
  filesystem executable's current QtTest cases and staged-header poison.
- `./tools/validate-docs`: **107 documents/navigation passed**.
- `./tools/check-source-shape`: **1555 files checked**; only the documented
  unrelated 539-line Display Color test warning.
- `git diff --check`, exact commit/tree/sole-parent provenance: passed.
- Detached candidate status, ignored-status filter, and whole-tree `.pyc/.pyo`
  scan are empty after removing validator-generated cache files; HEAD remained
  exact and unamended.

Passing happy-path tests do not mitigate either P1 because there is no injected
post-rename/post-unlink directory-sync failure and no post-open same-inode size
growth. Faraday should repair in the preserved writer worktree, commit a new
immutable descendant without amending this candidate, and request exact
rereview.
