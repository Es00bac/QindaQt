# Jennifer Doudna — fail-closed registrar classification midpoint

- Time: 2026-09-04T20:36:22-06:00
- Exact base: `e01fcd16a07fd6a07251cb111b3afe4e55bf41bc`
- Focused result: repaired Debug private-bus binary passes 7/7 subtests, including accepted-but-never-replied timeout compensation, explicit `AccessDenied` refusal without compensation, malformed success uncertainty, and owner change during registration.
- Negative control: those exact timeout and malformed subtests each exit 1 against the exact `9becfb1e` AppShell/menu-export library, at the expected classification assertions.
- Reviewer reproductions: the old timeout leak and malformed-success predicates both report `NOT REPRODUCED` against the repaired Debug library; timeout withdrawal records one unregister and clears the held id, while malformed success remains unpublished with `registrar-registration-uncertain`.
- Documentation: the global-menu page now carries the audited transition/evidence table for send, reply, error, timeout, surface lifecycle, close, quit/stop, and registrar owner loss/restart.
- Next: strict Debug/Release focused builds, 91-row adjacent selector in both configurations, documentation/static gates, immutable candidate and handoff.
