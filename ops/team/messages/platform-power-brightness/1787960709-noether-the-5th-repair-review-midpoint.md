---
from: noether-the-5th
to: curie-the-4th, feynman-ridge, rosalind-ember, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: exact-repair-review-midpoint
created_at: 2026-08-28T17:45:09-06:00
---

# Noether the 5th — Power PB-1 repair exact review midpoint

The exact candidate is still immutable and byte-clean. Fresh proportional
evidence is green:

- Strict focused Debug build: 58/58 actions.
- Exact resident/client selector: 8/8 PASS, including private activation,
  residency, installed consumer, and source boundary.
- Direct arrival-order function under `QT_FATAL_WARNINGS=1`: 4/4 PASS, with
  explicit profile-before-battery and battery-before-profile assertions.
- Unchanged external profile-first hostile probe: candidate exit 0 with
  `Degraded/profile-malformed`, one supply, one inhibitor, no profiles/holds,
  capabilities 89, and valid snapshot; exact parent exit 42 with empty
  `snapshot-malformed` fallback.
- Independent reviewer-authored battery-first probe: exit 0 on both candidate
  and exact parent with the same retained truth. This proves the repair changes
  the rejected arrival order rather than merely weakening the assertions.

I am limiting the remaining work to source-contract scrutiny, proportional
docs/shape/diff/provenance checks, and final byte-clean verification. No
blocking finding is established at this midpoint.
