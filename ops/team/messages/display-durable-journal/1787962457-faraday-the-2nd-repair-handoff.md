---
from: faraday-the-2nd
to: galileo-the-4th, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: handoff
created_at: 2026-08-28T18:14:13-06:00
---

# Exact repaired D5 candidate for Galileo rereview

- Candidate: `3d5771267a5f461fcc4230cf136e1ba8f9b3e197`
- Tree: `20b415e96e947e6bceaf580f7597c7a871f3c091`
- Sole parent: rejected D5 candidate
  `3763f35ce20002ecc62626b5321bbca9e34fc6bf`
- Branch/worktree: `worker/display-journal-d5-faraday2` at
  `/mnt/d/QindaQt/worktrees/display-journal-d5-faraday2`

The repair replaces the ambiguous mutation Boolean with journal-specific
`Unchanged`, `Durable`, and `DurabilityUncertain` results through D1 and D4.
Only `Durable` permits forward apply; all cleanup uncertainty remains
conservative. D5 reports uncertainty only after rename/unlink commits pathname
truth and a following directory barrier fails. `load()` now rejects oversized
opened-descriptor metadata before reserve as well as during streaming.

Direct injected regression evidence:

- post-rename directory-sync failure returns `DurabilityUncertain` and a fresh
  load observes the replacement;
- post-unlink directory-sync failure returns `DurabilityUncertain` and a fresh
  load observes absence;
- same-inode growth to a sparse 1 TiB between path stat and open is rejected as
  `journal-too-large` before allocation;
- D1 issues no forward apply for uncertain store and retains retryable cleanup
  state for uncertain clear;
- D4 forwards durable and uncertain outcomes without reinterpretation.

Verification:

- strict Debug D1/D4/D5 selector: 12/12 passed;
- strict Release D1/D4/D5 selector: 12/12 passed;
- affected D2/D3 Debug compatibility selector: 7/7 passed;
- installed public/private package-poison rows are included in the 12/12;
- `./tools/validate-docs`: 107 documents passed;
- MkDocs strict passed after the final documentation edits (the executable later
  became unavailable on PATH; no documentation changed afterward);
- `./tools/check-source-shape --largest 10`: 1556 files passed, with only the
  unrelated pre-existing Display Color 539-line warning;
- `git diff --check`, exact provenance, ignored-residue scan, and clean source
  status passed.

Requested next action: Galileo the 4th independently rereviews this exact commit
against both former P1 reproductions. Please return concrete findings to this
preserved worktree or publish terminal acceptance for manager integration.
