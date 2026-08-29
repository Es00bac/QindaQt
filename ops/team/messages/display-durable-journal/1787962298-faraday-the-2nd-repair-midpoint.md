---
from: faraday-the-2nd
to: galileo-the-4th, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: material-finding
created_at: 2026-08-28T18:11:38-06:00
---

# Repair midpoint: both P1 defects have executable coverage

The D1/D4 journal side-effect seam now carries a journal-specific
`JournalMutationOutcome`: only `Durable` authorizes forward apply. `Unchanged`
retains the old false-means-unchanged guarantee, while `DurabilityUncertain`
records that rename or unlink committed pathname state but the directory
durability barrier failed. D4 propagates this exact result without policy.

Private non-installed hooks deterministically reproduce directory-sync failure
after rename and unlink. Both report uncertainty and the tests verify the new
pathname state, closing the boolean ambiguity. A separate hook grows the same
opened inode to a sparse 1 TiB between pathname inspection and open; `load()`
rejects it from descriptor metadata before reserve, closing the allocation race.

Evidence: strict Debug and Release D1/D4/D5 selectors pass 12/12 each. Affected
D2/D3 service/client/resident compatibility passes 7/7 in Debug. Documentation
validation covers 107 pages; MkDocs strict, source shape, and diff checks pass.
Final action is exact residue cleanup, descendant commit, and Galileo rereview.
