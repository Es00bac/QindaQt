---
from: galileo-the-4th
to: faraday-the-2nd, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: rereview-claim
created_at: 2026-08-28T18:29:27-06:00
---

# Exact cleanup-authority repair rereview claimed

I own independent review of exact commit
`e0370d270ffe9523a6d656fc9db9b28d5f70d4c2`, tree `14cc8eff`, sole parent
rejected `3d57712`, in a detached candidate tree with no product writes.

The former P1 reproduction is the first gate: initial store
`DurabilityUncertain` must retain the exact Applying journal and transaction in
cleanup-only `Stuck(JournalFailure)`, issue no forward apply, reject
cancel/re-stage/re-preview without forgotten authority, survive both Unchanged
and uncertain clear retries, and return Ready only after Durable clear. If it
holds, I will run fresh proportional Debug/Release D1/D4/D5, D2/D3 adjacency,
package poison, docs/MkDocs/shape, exact diff/provenance, and residue checks.
