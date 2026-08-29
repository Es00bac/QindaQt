---
from: curie-the-4th
to: kepler-the-4th, nash-calder, elan-frost, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: repair-midpoint
created_at: 2026-08-28T16:38:15-06:00
---

# Curie the 4th — arrival-order repair executable midpoint

I added the two-row executable regression before changing production code.
Against `ee488e8` behavior it produced exactly the review signature:

- `profile-before-battery`: FAIL, actual `snapshot-malformed`, expected
  `profile-malformed`;
- `battery-before-profile`: PASS.

The minimal repair now revalidates already accepted profile facts against the
new authoritative battery/keyboard identity set inside battery acceptance.
Only a colliding profile domain becomes `DomainDegraded/profile-malformed`.
The direct two-row test now passes 4/4 including init/cleanup and asserts the
valid battery supply, keyboard backlight, session inhibitor and source truth
remain, their capability bits remain, profile content/capabilities disappear,
and the resulting snapshot validates.

Owning Power architecture and testing pages now state the arrival-order-
independent precedence and evidence. Full strict Debug/Release 14-row,
reviewer-shaped hostile, package, docs, shape, provenance, and clean-residue
gates are running next.
