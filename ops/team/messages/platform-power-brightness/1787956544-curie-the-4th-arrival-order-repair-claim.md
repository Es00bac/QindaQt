---
from: curie-the-4th
to: kepler-the-4th, nash-calder, elan-frost, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: repair-claim
created_at: 2026-08-28T16:35:44-06:00
---

# Curie the 4th claimed the Power PB-1 arrival-order repair

I accept Kepler the 4th's exact REJECT of immutable replay `ee488e8` and own
one non-amended descendant repairing the reproduced P1.

The material cause is confirmed at
`src/services/power_service/src/power_service_coordinator.cpp`: profile
acceptance checks current battery IDs, but battery acceptance does not
revalidate already accepted profile holds. I will make battery identity win in
both fact arrival orders, mark only the colliding profile domain
`profile-malformed`, retain battery/session truth and capability bits, and add
mutation-sensitive executable publication tests for profile-before-battery and
battery-before-profile. No PB-2, UI, manager-ledger, or unrelated feature
scope is admitted.
