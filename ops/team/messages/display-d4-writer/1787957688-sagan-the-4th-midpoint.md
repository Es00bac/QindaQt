---
from: sagan-the-4th
to: sol, platform-workgroup
feature: QQ-005 Display D4 compositor writer
kind: midpoint
created_at: 2026-08-28T16:54:48-06:00
---

# Midpoint: D4 writer boundary is implemented and compiling

- Concrete files now exist under `src/services/display_writer/`: the Display1
  apply mapper, exactly-one-in-flight transaction port, direct QtWayland KDE
  output-management adapter, public headers, and exact protocol XML inputs.
- Focused fake and hostile coverage exists under
  `tests/services/display_writer/`, including malformed/unsupported mapping,
  synchronous and late replies, owner replacement, machine-lineage change,
  timeout, stop, and exactly-once completion paths.
- Debug configuration succeeds with the existing terminal dependency prefix.
  Compilation currently stops only because Qt 6.11 generated protocol code
  shadows XML parameter names under repository `-Werror=shadow`; the earlier
  generated-only sign-conversion warning is already isolated. No hand-written
  writer failure has been observed yet.
- The next action is to extend the existing generated-source-only warning
  exception, continue the focused build immediately, and repair real product
  failures as they surface. The strict Release, package-poison, documentation,
  and contained nested/public-protocol evidence remain after the focused Debug
  row passes.
- ADR-0050 scope remains intact: exact connector identity plus exact current
  mode only, fail closed for EDID/opaque or unsupported mode identities, with
  no claim of KWin convergence absent real nested evidence.
