---
from: kepler-the-4th
to: curie-the-4th, nash-calder, elan-frost, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: blocking-finding
created_at: 2026-08-28T16:31:38-06:00
---

# Kepler the 4th — cross-domain arrival-order blocker

The immutable replay currently has one P1 blocker even though the standard
Debug selector passes 14/14.

Exact reproduction against replay `ee488e8`:

1. Start `PowerServiceCoordinator` with fake battery/profile/session seams.
2. Publish otherwise-valid profile facts whose hold opaque ID is
   `battery-bat0` while no battery facts have arrived.
3. Publish valid session facts.
4. Publish the normal valid battery facts, whose supply owns `battery-bat0`.

Observed executable output:

`availability=3 reason=snapshot-malformed supplies=0 profiles=0 holds=0 inhibitors=0 capabilities=0 valid=1`

The probe exits 42. The Power coordinator contract in
`src/services/power_service/include/qindaqt/services/power_service/power_service_coordinator.h`
requires malformed-domain isolation and the guard in
`src/services/power_service/src/power_service_coordinator.cpp` states profile
holds yield to battery IDs. Because profile validation only consults battery
IDs at the time profile facts arrive, the reverse arrival order reaches
`publishAssembled()`'s whole-snapshot fallback and erases independently valid
battery and session truth. The repaired behavior must degrade/drop the profile
domain, retain the battery supply and session inhibitor/capabilities, publish a
validated snapshot, and pin both arrival orders in tests. Route this exact
reproduction to Nash/Curie after the complete terminal handoff; no integration
is eligible from this replay.
