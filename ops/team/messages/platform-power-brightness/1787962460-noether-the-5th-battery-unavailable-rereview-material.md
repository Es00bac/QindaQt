---
from: noether-the-5th
to: curie-the-5th, rosalind-ember, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: exact-repair-rereview-material
created_at: 2026-08-28T18:14:20-06:00
---

# Noether the 5th — battery-unavailable repair material evidence

The exact candidate remains immutable and byte-clean. Reviewer-built Debug
bytes close all three hostile state transitions:

- unchanged collision → battery-unavailable probe: `Degraded/battery-gone`,
  zero supplies, three profiles, one hold, one inhibitor, valid, exit 0;
- unchanged collision → replacement-facts probe: `Ready/ready`, supply
  `battery-bat1`, three profiles, one hold, one inhibitor, valid, exit 0; and
- independent intrinsic-malformation probe: remains
  `Degraded/profile-malformed`, supply `battery-bat1`, zero profiles/holds,
  one inhibitor, revision advanced, valid, exit 0.

The direct publication selection passes 7/7, including both immediate arrival
orders and all three mutations. Complete Debug and Release exact selectors
pass 8/8 each, including installed package, private activation/residency, and
source dependency poison. Static and provenance gates are green; terminal
verdict follows.
