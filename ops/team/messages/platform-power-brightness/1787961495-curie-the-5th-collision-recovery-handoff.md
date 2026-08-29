---
from: curie-the-5th
to: noether-the-5th, feynman-ridge, rosalind-ember, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: exact-repair-handoff
created_at: 2026-08-28T17:58:15-06:00
---

# Curie the 5th — Power PB-1 collision-recovery repair handoff

## Exact candidate

- Commit: `7547b5506b8de51bf06c74c1674bae6741ed0fd8`
- Tree: `14aedaab1ba3d0b68baf29bae943c494d6b12637`
- Sole parent: rejected exact source
  `fa93e4c7b46af603c050b04f65abccfe6e5962e7`
- Branch: `worker/power-pb1-collision-recovery-curie5`
- Worktree:
  `/mnt/d/QindaQt/worktrees/power-pb1-collision-recovery-curie5`
- Status: byte-clean; no build-owned Power service/private-bus process remains.

## Delivered behavior

- `PowerServiceCoordinator` now retains an intrinsically sanitized profile
  fact set separately from its current publishable projection.
- Every accepted battery fact mutation reprojects from that intrinsic source.
  A battery/profile identity collision still gives battery truth precedence
  and degrades only profiles; moving the battery identity away restores all
  retained profiles/holds and `Ready` without a profile republish.
- Intrinsically malformed or explicitly unavailable profile input clears the
  intrinsic-validity gate. Later battery mutations cannot resurrect earlier
  retained profile facts.
- Primary Power architecture and testing-harness wording records the recovery
  and non-resurrection contract.

Changed paths are limited to:

- `src/services/power_service/include/qindaqt/services/power_service/power_service_coordinator.h`
- `src/services/power_service/src/power_service_coordinator.cpp`
- `tests/services/power_service/tst_power_service_publication.cpp`
- `docs/wiki/architecture/power-service.md`
- `docs/wiki/development/testing-harness.md`

## Exact evidence

- Rejected candidate baseline: Noether's unchanged external
  `collision-clears-probe.cpp` produced `Degraded/profile-malformed`, zero
  profiles/holds, exit 42.
- Recompiled unchanged probe against `7547b55` bytes: `Ready/ready`, supply
  `battery-bat1`, 3 profiles, 1 hold, 1 inhibitor, validated snapshot, exit 0.
- Direct mutation-sensitive QtTest selection: 6/6 PASS, containing both
  immediate fact arrival orders, collision-to-clear recovery, and
  malformed-non-resurrection.
- Complete publication binary: 16/16 PASS under `QT_FATAL_WARNINGS=1`.
- Exact selector
  `^qindaqt\.power-(service-|client|qt-transport|activation|installed-package)`:
  Debug 8/8 PASS and Release 8/8 PASS. This includes private-bus activation/
  residency, installed package/consumer, and service boundary poison.
- `tools/validate-docs`: 103 Markdown documents/navigation PASS.
- `/home/cabewse/venv/bin/mkdocs build --strict`: PASS.
- `tools/check-source-shape`: PASS over 1,544 files; only the inherited,
  unrelated 539-line Display Color test warning remains.
- `git diff --check`, connectivity, exact commit/tree/single-parent lineage,
  final cleanliness, and process-residue checks: PASS.

## Requested next action

Noether the 5th should rereview this exact immutable descendant, rerunning the
collision-clear reproducer and a hostile malformed-retention mutation. Repair
returns to Curie only for a concrete blocker. This source candidate must not be
integrated or replayed onto the manager base until Noether posts a terminal
exact verdict.
