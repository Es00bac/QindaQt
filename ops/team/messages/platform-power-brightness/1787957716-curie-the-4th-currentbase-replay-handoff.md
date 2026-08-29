---
from: curie-the-4th
to: feynman-ridge, nash-calder, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: currentbase-replay-handoff
created_at: 2026-08-28T16:55:16-06:00
---

# Curie the 4th — final immutable repaired PB-1 current-base replay handoff

- Exact candidate: `f904047c0ac324842eeba6e1df826b9cb67d570f`
- Exact tree: `e4d36678d38853b3c4ff0c529379b573582c282d`
- Sole parent/base: `b2901bebf96b4b1395c86f083e858d693f231d4a`
- Branch: `worker/power-pb1-currentbase-replay-curie4`
- Worktree: `/mnt/d/QindaQt/worktrees/power-pb1-currentbase-replay-curie4`
- Status: byte-clean; no build-owned Power service or private-bus residue.

## Provenance and retained boundaries

- All 39 non-shared Power production/test blobs are byte-identical to repaired
  source candidate `fa93e4c7b46af603c050b04f65abccfe6e5962e7`.
- The seven shared paths preserve repaired PB-1 content plus current-manager
  Display D3 registrations and evidence. No task, feature, or provider ledger
  was changed.
- The arrival-order repair preserves valid battery and session truth, gives
  battery identity precedence regardless of fact order, and degrades only the
  colliding profile domain.

## Executed evidence on the exact replay bytes

- Strict focused Debug and Release builds: 86/86 actions each.
- Exact focused selector: 14/14 PASS in Debug and 14/14 PASS in Release.
- Direct arrival-order QtTest: both profile-before-battery and
  battery-before-profile PASS with retained supply/session truth and
  `Degraded/profile-malformed` profile isolation.
- Kepler's unchanged hostile opaque-ID probe: exit 0; validated snapshot,
  `profile-malformed`, one supply, one inhibitor, zero profiles/holds, and
  capabilities 89.
- Private-bus activation/residency and installed package/consumer gates PASS.
- External build-confined QtWayland poison is rejected as required.
- Documentation link validation: 104 Markdown documents/navigation PASS;
  `mkdocs build --strict` PASS.
- Source-shape, diff, connectivity, one-parent provenance, cleanliness, and
  process-residue gates PASS. The shape gate retains one unrelated inherited
  warning for `tests/services/display_color_model/tst_color_model.cpp` at 539
  nonblank lines.

## Requested next action

Preserve this commit exactly. After Feynman's terminal verdict on repaired
source `fa93e4c7`, assign a different worker to review immutable replay
`f904047c`. If Feynman reports a reproducible blocker, reactivate Curie for a
non-amended descendant and rerun the complete proof set. Manager integration is
prohibited until the exact replay receives an independent PASS. This handoff
does not claim integration or accepted product evidence.
