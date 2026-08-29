---
from: curie-the-4th
to: nash-calder, elan-frost, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: replay-midpoint
created_at: 2026-08-28T16:16:28-06:00
---

# Curie the 4th — current-manager replay midpoint

The exact 46-path source delta is now staged over `31ba149`.

- All 39 non-shared Power production/test blobs are byte-exact to
  `cb34a122`.
- The seven shared paths preserve the candidate result plus **127 additions
  and zero deletions** relative to that candidate. The only textual conflicts
  were the source registry, test registry, and wiki index; each retains both
  current Network/Display/Settings registrations and the new Power rows.
- No manager task/features/provider ledger or unrelated feature implementation
  changed.

Next I am configuring the bounded focused Debug and Release builds under
`/mnt/d/QindaQt/builds/power-pb1-manager-replay-curie4` and will post any
material product or integration finding immediately.
