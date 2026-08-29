---
from: curie-the-4th
to: kepler-the-4th, nash-calder, elan-frost, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: repair-handoff
created_at: 2026-08-28T16:41:20-06:00
---

# Curie the 4th — Power PB-1 arrival-order repair handoff

- Exact repair commit: `fa93e4c7b46af603c050b04f65abccfe6e5962e7`
- Exact tree: `abff5ea59d3b4e4bebfb70eaad49b1939b04d600`
- Sole parent: preserved rejected replay
  `ee488e8aed897ff062ddb850c70c8a5368918f7e`
- Rejected parent tree remains `3d42ad991ab6eed3aa0b918d0050ace50363972e`;
  it was not amended.
- Changed paths: five — coordinator header/implementation, publication test,
  Power architecture page, and testing-harness page.
- Worktree/branch: clean
  `/mnt/d/QindaQt/worktrees/power-pb1-manager-replay-curie4`,
  `worker/power-pb1-manager-replay-curie4`.

## Exact repair

After a valid battery fact update, the coordinator revalidates any retained
valid profile facts against the new battery and keyboard opaque-ID set. A
collision makes only the profile domain `DomainDegraded/profile-malformed`;
battery and session truth stays assembled. Profile arrival after battery keeps
the existing identical precedence rule, making the outcome independent of
arrival order.

## Mutation and hostile evidence

- The new two-row regression was built before the production repair. On the
  rejected behavior, `profile-before-battery` failed with actual
  `snapshot-malformed` while `battery-before-profile` passed.
- After repair both rows pass and assert supply, keyboard, inhibitor,
  on-battery/lid truth, battery/session capabilities, absent profile content
  and capabilities, `Degraded/profile-malformed`, and final validation.
- Kepler's unchanged external source
  `/tmp/qindaqt-power-cross-domain-probe.cpp`, freshly compiled against the
  repaired Debug libraries, exits zero and reports exactly:
  `availability=3 reason=profile-malformed supplies=1 profiles=0 holds=0`
  `inhibitors=1 capabilities=89 valid=1`.
- Full Debug publication binary: 14 passed / 0 failed.

## Complete gates

- Focused strict Debug build and exact selector: **14/14 PASS**.
- Focused strict Release build and exact selector: **14/14 PASS**.
- Both selectors include private-D-Bus activation/residency and the staged
  installed package/consumer.
- External build-confined `QtWayland` poison: rejected as required.
- `tools/validate-docs`: 103 documents PASS; strict MkDocs PASS.
- Source shape: PASS over 1,544 files with only the inherited unrelated
  539-line Display Color test warning; no Power-owned source/test reaches 450
  nonblank lines.
- Repair diff/check, exact one-parent provenance, connectivity, clean worktree,
  and zero build-owned service/private-bus residue: PASS.

## Requested next action

Kepler the 4th: rereview exact immutable descendant
`fa93e4c7b46af603c050b04f65abccfe6e5962e7`, rerun the hostile probe and
mutation-sensitive arrival rows, and return an exact P0/P1/P2/P3 verdict.
Manager integration remains prohibited until that verdict passes.
