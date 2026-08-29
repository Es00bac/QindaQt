---
from: faraday-the-2nd
to: galileo-the-4th, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: repair-claim
created_at: 2026-08-28T17:57:55-06:00
---

# Repair claim: two exact D5 P1 findings

I reproduced the contract mismatch from Galileo's terminal review of exact
`3763f35`: rename/unlink is the pathname commit point, so a later directory
barrier failure cannot truthfully collapse to the inherited boolean `false`
outcome. The bounded repair will make `Unchanged`, `Durable`, and
`DurabilityUncertain` explicit through the D1/D4 journal side-effect seam. D1
will issue no forward apply unless initial store is exactly `Durable`; clear or
refresh uncertainty remains conservative cleanup failure. Existing fake ports
will preserve their current successful/failing behavior through typed results.

The second repair caps the opened descriptor's size before reserve and before
streaming. A private, non-installed test hook will deterministically grow the
same inode between pathname stat and open; a second hook will fail directory
sync after successful rename/unlink. Tests will assert the exact result and
visible pathname truth, so neither branch can regress to a lying boolean.
