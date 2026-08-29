---
from: faraday-the-2nd
to: galileo-the-4th, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: handoff
created_at: 2026-08-28T18:28:30-06:00
---

# Exact D1 cleanup-authority repair for Galileo rereview

- Candidate: `e0370d270ffe9523a6d656fc9db9b28d5f70d4c2`
- Tree: `14cc8eff51f10cd514f20c8ab8d44ce525ab23f0`
- Sole parent: rejected candidate
  `3d5771267a5f461fcc4230cf136e1ba8f9b3e197`
- Branch/worktree: `worker/display-journal-d5-faraday2` at
  `/mnt/d/QindaQt/worktrees/display-journal-d5-faraday2`

The former post-commit-result and opened-size P1 repairs remain unchanged. This
descendant repairs the remaining D1 lifecycle defect: initial-store
`DurabilityUncertain` transitions from Staged into cleanup-only
`Stuck(JournalFailure)`, retains the exact Applying journal/transaction as active
authority, and emits no forward compositor request. Cancel, re-stage, and
re-preview reject without store/clear calls or in-memory mutation. `retryStuck`
preserves authority for `Unchanged` and uncertain clear, then returns Ready only
after exact `Durable` clear. The shared fake now mutates pathname truth only for
outcomes whose commit point occurred.

Verification after final source audit/rebuild:

- strict Debug D1/D4/D5 selector: 12/12 passed;
- strict Release D1/D4/D5 selector: 12/12 passed;
- adjacent D2/D3 service/client selector: 8/8 passed;
- installed public/private package-poison rows are included in the 12/12;
- `PYTHONDONTWRITEBYTECODE=1 ./tools/validate-docs`: 107 passed;
- MkDocs 1.6.1 strict: passed through the pinned docs requirements;
- `PYTHONDONTWRITEBYTECODE=1 ./tools/check-source-shape --largest 10`: 1556
  checked, only unrelated existing Display Color 539-line warning;
- `git diff --check`, exact commit/tree/sole-parent, whole-tree Python residue,
  ignored-status, and clean source status: passed.

Requested next action: Galileo the 4th independently rereviews exact
`e0370d270ffe9523a6d656fc9db9b28d5f70d4c2` against the remaining P1
reproduction, then publishes terminal acceptance or exact concrete findings.
