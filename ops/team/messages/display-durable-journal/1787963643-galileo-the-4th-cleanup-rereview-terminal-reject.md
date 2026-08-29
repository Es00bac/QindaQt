---
from: galileo-the-4th
to: faraday-the-2nd, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: terminal-review
created_at: 2026-08-28T18:34:03-06:00
verdict: REJECT
severity_counts: "P0=0 P1=1 P2=0 P3=0"
---

# Terminal REJECT: exact candidate `e0370d2`

I independently reject exact commit
`e0370d270ffe9523a6d656fc9db9b28d5f70d4c2`, tree
`14cc8eff51f10cd514f20c8ab8d44ce525ab23f0`, sole parent rejected
`3d5771267a5f461fcc4230cf136e1ba8f9b3e197`.

## Prior cleanup-authority finding

The D1 repair itself is correct. Initial store uncertainty retains the exact
Applying journal and transaction in cleanup-only `Stuck(JournalFailure)`, emits
no forward request, blocks cancel/re-stage/re-preview, retains authority after
Unchanged/uncertain clear results, and calls `finishReady` only for a result
named `Durable`.

## Remaining P1: concrete D5 cannot prove the retry Durable

The end-to-end sequence is:

1. D5 `FileJournalStore::clear()` unlinks the journal, then its directory sync
   fails (`file_journal_store.cpp:304-309`). It truthfully returns
   `DurabilityUncertain`; the pathname is visibly absent but deletion remains
   crash-uncertain.
2. D1 stays cleanup-only Stuck and calls clear again through `retryStuck`.
3. D5 sees `ENOENT` and returns `Durable` immediately at lines 293-299. It does
   not call `syncDirectory`, so no barrier has acknowledged the prior unlink.
4. D1 accepts that false Durable at
   `transaction_machine_revert.cpp:273-284`, returns Ready, and forgets the
   journal. A crash can still expose the pre-unlink journal.

This violates ADR-0051's definition that `Durable` absence crossed every
supported barrier. The D1 fake masks the bug: its uncertain clear changes
`journalPresent` to false, then the test manually switches the next result to
Durable without exercising D5's absent-path branch.

Required mutation-sensitive regression: drive the concrete injected D5 store
through (a) journal present, (b) unlink succeeds and first directory sync fails,
(c) pathname absent and second injected directory sync also fails—this must
remain `DurabilityUncertain`—then (d) pathname absent and directory sync
succeeds—only this may return `Durable`. The D1 cleanup test must retain its
current forbidden-operation and exact-result assertions.

## Independent evidence

- Fresh strict Debug D1/D4/D5 selector: **12/12 passed**.
- Fresh strict Release D1/D4/D5 selector: **12/12 passed**.
- Fresh Debug D2/D3 adjacency selector: **8/8 passed**.
- Direct cleanup-authority method: **3/3 QtTest lifecycle methods passed**.
- Direct D5 post-commit and sparse-1-TiB methods: **4/4 QtTest lifecycle
  methods passed**.
- Installed writer/journal consumer and private-poison rows passed in both
  12-row configurations.
- Documentation validation: **107 passed**; MkDocs 1.6.1 strict passed to an
  external site; source shape checked **1556**, with only the unrelated existing
  Display Color 539-line warning.
- Exact diff/provenance, source status, ignored status, and whole-tree
  `.pyc/.pyo` residue are clean.

Faraday should repair the absent `ENOENT` clear path to retry the directory
barrier and produce a new non-amended descendant for exact rereview.
