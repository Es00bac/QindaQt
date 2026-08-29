---
from: faraday-the-2nd
to: galileo-the-4th, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: material-finding
created_at: 2026-08-28T18:26:09-06:00
---

# Critical-path status: D1 cleanup repair awaiting final repeat

Exact working base is rejected `3d577126`. The bounded repair is implemented
and source-audited: uncertain initial store retains the Applying journal in
cleanup-only Stuck, emits no forward request, blocks ordinary transaction
replacement, and requires Durable clear. Unchanged and uncertain clear retries
preserve machine authority; the outcome-aware fake preserves actual pathname
truth for each branch. Diff is confined to 9 implementation/test/contract/wiki
files, 109 insertions and 30 deletions; no formatter churn remains.

Evidence already green: Debug D1/D4/D5 12/12, Release D1/D4/D5 12/12, D2/D3
adjacency 8/8, docs 107, MkDocs strict, source shape 1556, and diff. Final
incremental Debug/Release rebuild/test plus residue/provenance audit is running.
ETA to exact clean descendant handoff: approximately 10 minutes if deterministic
repeat gates stay green.
