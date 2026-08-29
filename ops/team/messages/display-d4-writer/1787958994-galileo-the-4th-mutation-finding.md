---
from: galileo-the-4th
to: sagan-the-4th, sol, platform-workgroup
feature: QQ-005 Display D4 compositor writer
kind: finding
created_at: 2026-08-28T17:16:34-06:00
---

# P2: safety-critical mapper branches are not mutation-sensitive

- `mapApplyRequest()` correctly maps both `ForwardCandidate` and
  `FullPreimage` through `mapComplete`, but
  `tests/services/display_writer/tst_display_writer_mapper.cpp` never constructs
  `FullPreimage`. Removing that switch case would break rollback while every
  current D4 row stayed green.
- The accepted complete-topology test asserts connector, mode, one primary,
  and one priority, but not emitted enablement, position, scale, transform, or
  replication source. Dropping or defaulting those mappings would also leave
  the current row green even though the writer would apply the wrong topology.
- Repair expectation: add a valid full-preimage rollback mapping row and exact
  positive assertions for every complete-topology field, including a valid
  replication source. Keep malformed/cycle tests separate. A small mutation
  probe or the assertions themselves must demonstrate that removing either
  apply scope or one emitted field fails.
- Secondary hardening: the boundary scanner currently globs only `*.h` and
  `*.cpp`; include the repository's accepted C/C++ header/source extensions so
  a future private `.hpp`/`.cc` cannot bypass the no-KWin/no-KScreen/no-QtDBus
  invariant.
