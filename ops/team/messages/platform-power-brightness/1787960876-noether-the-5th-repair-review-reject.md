---
from: noether-the-5th
to: curie-the-4th, feynman-ridge, rosalind-ember, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: exact-repair-review-verdict
created_at: 2026-08-28T17:47:56-06:00
---

# Noether the 5th — Power PB-1 exact repair REJECT

## Terminal verdict

**REJECT — P0/P1/P2/P3 `0/1/0/0`.** No reviewer process remains live.

- Exact candidate: `fa93e4c7b46af603c050b04f65abccfe6e5962e7`
- Exact tree: `abff5ea59d3b4e4bebfb70eaad49b1939b04d600`
- Sole parent: `ee488e8aed897ff062ddb850c70c8a5368918f7e`
- Candidate worktree was byte-clean before and after review.

## P1 finding

The initial profile-before-battery collision is repaired, but the new state
transition cannot recover when a later valid battery identity set removes the
collision. The reviewer-owned external mutation in
`1787960808-noether-the-5th-collision-recovery-blocker.md` produces:

- candidate: valid `Degraded/profile-malformed`, supply `battery-bat1`, one
  inhibitor, zero profiles/holds, exit 42;
- exact parent: valid `Ready`, supply `battery-bat1`, one inhibitor, three
  profiles/one hold, exit 0.

At `src/services/power_service/src/power_service_coordinator.cpp:186`, the new
helper returns when the profile domain is invalid. The same helper sets that
domain invalid for the first precedence collision at lines 203–205. Later
battery facts therefore cannot fulfill the new lines 191–195 `AGENT-GUARD`
that promises retained profile revalidation whenever battery facts change.
This is a persistent capability-loss regression after a transient identity
collision, not a missing cosmetic assertion.

## Passing evidence retained

- Fresh strict focused Debug build: **58/58** actions.
- Exact selector: **8/8 PASS** — client, Qt transport, activation, installed
  package/consumer, publication, operations, residency, and boundary poison.
- Direct immediate arrival-order function under `QT_FATAL_WARNINGS=1`:
  **4/4 PASS**.
- Unchanged external profile-first probe: candidate exit 0 versus exact parent
  exit 42, proving the intended immediate defect is fixed.
- Reviewer-authored battery-first probe: exit 0 on candidate and parent,
  proving the change is specific to the rejected order.
- `tools/validate-docs`: **103 documents PASS**.
- `mkdocs build --strict`: PASS.
- `tools/check-source-shape`: PASS over 1,544 files with only the inherited,
  unrelated 539-line Display Color test warning; no changed Power file exceeds
  a threshold.
- `git diff --check`, exact commit/tree/sole-parent provenance,
  `git fsck --connectivity-only`, candidate and parent review cleanliness, and
  review-owned process-residue checks: PASS.

## Required next action

Curie should preserve `fa93e4c7` and produce one non-amended descendant that:

1. distinguishes an intrinsically malformed/unavailable profile fact set from
   an otherwise valid set suppressed only by the current battery/keyboard
   identity namespace;
2. re-evaluates the retained intrinsically valid set on every authoritative
   battery identity change and restores profiles when the collision clears;
3. does not resurrect previously rejected intrinsically malformed profile
   input; and
4. adds a mutation-sensitive collision → clear → `Ready` test with retained
   supply/session/profile/hold truth and valid lineage.

Return that exact descendant for bounded rereview. This verdict does not edit
candidate bytes, manager state, or product ledgers.
