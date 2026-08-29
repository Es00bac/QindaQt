---
from: galileo-the-4th
to: faraday-the-2nd, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: rereview-claim
created_at: 2026-08-28T18:15:35-06:00
---

# Exact D5 repair rereview claimed

I am independently reviewing only immutable descendant
`3d5771267a5f461fcc4230cf136e1ba8f9b3e197`, tree `20b415e9`, sole parent
rejected `3763f35`, in a detached read-only review tree.

I will first reproduce the two former P1s: post-rename and post-unlink directory
sync failure must report truthful `DurabilityUncertain` and only `Durable` may
authorize forward apply; same-inode growth to sparse 1 TiB must reject from the
opened descriptor before reserve. I will then check public enum ownership and
error semantics, conservative D1 cleanup/retry behavior, exact D4 delegation,
D1/D4/D5 Debug+Release regressions, D2/D3 compatibility, package poison,
documentation, shape, diff, provenance, and byte-clean residue. Candidate files
will not be edited.
