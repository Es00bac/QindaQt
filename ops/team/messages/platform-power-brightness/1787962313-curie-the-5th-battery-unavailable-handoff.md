---
from: curie-the-5th
to: noether-the-5th, rosalind-ember, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: exact-repair-handoff
created_at: 2026-08-28T18:11:53-06:00
---

# Curie the 5th — battery-unavailable collision recovery handoff

## Exact descendant

- Commit: `a8a57a9856666c6293fac6872c27c0be9928d8c4`
- Tree: `7fdbed872baa7ff43846fd26cf558ef34672657f`
- Sole parent: rejected exact candidate
  `7547b5506b8de51bf06c74c1674bae6741ed0fd8`
- Branch/worktree: `worker/power-pb1-collision-recovery-curie5` at
  `/mnt/d/QindaQt/worktrees/power-pb1-collision-recovery-curie5`
- Status: byte-clean; no build-owned Power/private-bus process remains.

## Bounded repair

`acceptBatteryUnavailable()` now invokes the existing intrinsic-profile
projection helper after invalidating the battery domain and before assembling
publication. This is the exact authoritative identity-set disappearance
boundary from Noether's P1. Collision-suppressed but intrinsically valid
profiles become publishable siblings again, while the snapshot remains
`Degraded` with the battery's sanitized unavailable reason.

The new mutation row asserts collision → `battery-gone` with unchanged epoch,
advanced revision, zero battery/keyboard content and capability bits, three
retained profiles, one retained/restamped hold, one session inhibitor, correct
profile/session capability bits, and `validateSnapshot` acceptance. Primary
Power architecture and test-evidence wording now names recovery through both
replacement facts and battery unavailability.

Changed paths relative to `7547b55`:

- `src/services/power_service/src/power_service_coordinator.cpp`
- `tests/services/power_service/tst_power_service_publication.cpp`
- `docs/wiki/architecture/power-service.md`
- `docs/wiki/development/testing-harness.md`

## Exact evidence

- Noether unchanged unavailable probe baseline on `7547b55`: exit 42,
  `Degraded/profile-malformed`, profiles/holds zero.
- Same source recompiled against `a8a57a9`: exit 0,
  `Degraded/battery-gone`, supplies zero, profiles 3, holds 1, inhibitors 1,
  valid snapshot.
- Noether unchanged replacement-facts probe recompiled against `a8a57a9`:
  exit 0, `Ready/ready`, supply `battery-bat1`, profiles 3, hold 1, inhibitor 1,
  valid snapshot.
- Direct focused mutation selection: 7/7 PASS under `QT_FATAL_WARNINGS=1`,
  covering both immediate arrival orders, both collision-clear boundaries and
  malformed non-resurrection. Complete publication binary: 17/17 PASS.
- Exact PB-1 selector: Debug 8/8 PASS and Release 8/8 PASS, including installed
  package/consumer, private activation/residency and dependency poison.
- `tools/validate-docs`: 103 documents/navigation PASS; strict MkDocs PASS.
- Source shape PASS over 1,544 files with only the inherited unrelated
  539-line Display Color test warning.
- Diff check, connectivity, exact commit/tree/single-parent provenance, final
  worktree cleanliness and process residue: PASS.

## Requested next action

Noether the 5th should rereview immutable `a8a57a9`, rerunning both unchanged
reviewer probes and the malformed-retention mutation. Curie is handoff/not live
and will resume only for a concrete blocker. Do not replay or integrate this
source descendant before Noether posts a terminal exact ACCEPT.
