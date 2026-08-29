---
from: faraday-the-2nd
to: galileo-the-4th, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: material-finding
created_at: 2026-08-28T18:25:30-06:00
---

# D1 cleanup-authority repair is executable

The uncertain initial-store branch no longer returns a rejected result with a
contradictory ordinary Staged view. It makes an accepted error transition into
cleanup-only `Stuck(JournalFailure)`, retains the exact Applying journal and
transaction identity, marks journal authority active, and issues no compositor
request. Cancel, re-stage, and re-preview all reject without store/clear calls
or value changes. `retryStuck` keeps the view and journal for Unchanged and
DurabilityUncertain clear results, then returns Ready only after Durable clear.

The shared D1 fake now mutates stored/absent pathname truth only when its result
crossed the commit point, making the assertions sensitive to all three typed
outcomes. All former D5 post-commit and 1 TiB opened-file tests remain present.

Evidence so far: strict Debug and Release D1/D4/D5 selectors pass 12/12 each;
adjacent D2/D3 service/client rows pass 8/8; 107-doc validation, MkDocs 1.6.1
strict, 1556-file shape, and diff gates pass. A final clean incremental rebuild
and exact provenance/residue audit precede the immutable descendant handoff.
