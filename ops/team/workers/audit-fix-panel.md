# Audit panel fixes

- Status: waiting — launcher startup/recovery status and skipped-entry presentation verified; awaiting independent review.
- Base: 49210a4c
- Ownership: launcher persistence key/schema contract, settings JSON encoding, focused tests and associated ADR/wiki.

## Updates

- 2026-09-05T15:55:06Z — Claimed isolated worktree; identified missing workspace/overview command boundary and requested manager follow-on for non-equivalent controls.

- 2026-09-05T16:02:25Z — QML regression suite passes30/30; launcher/task dispatchers5/5 each. Isolated preview build in progress; mkdocs strict and documentation links pass. Overflow now retains small controls and reveals keyboard-focused children. Seven preset equivalents migrated; manager routed remaining semantic controls.

- 2026-09-05T16:05:44Z — Isolated preview and focused C++ test build passed. Six CTest gates passed, including12 capture rows covering all10 profiles plus resolution/theme cases. QML suite30/30 and direct task/launcher dispatcher5/5 each; mkdocs strict, link checker and source-shape passed.

- 2026-09-05T16:49:40Z — Claimed manager-started isolated launcher worktree; preserved existing changes. Identified same-owner/new-epoch restart fixture violating the client lineage contract; will use distinct service connection/object and also verify a fresh shell client. Prior preference review probes remain preserved in fix-panel worktree.

- 2026-09-05T16:54:27Z — Launcher persistence builds passed;18/18 focused Settings/launcher CTests passed, including disk pins/recents, distinct owner/epoch recovery and fresh shell-client reload. MkDocs strict,165-document link check and source-shape passed. Panel candidate previously accepted; preference review probes remain in prior worktree, which is waiting rather than claiming active work.

- 2026-09-05T17:06:10Z — Private desktop screenshot exposed startup availability text persisting after Ready. Stage schema is current; controller sets notice while initially Unavailable and never clears it on valid snapshots. Adding status assertions and targeted recovery fix; scope excludes scanner diagnostic presentation.

- 2026-09-05T17:12:33Z — Screenshot warning reproduced despite persistenceReady with real service. Availability-only notice now clears on confirmed baseline; rejection/malformed errors stay useful. Isolated malformed files no longer show a global banner when applications remain usable; debug diagnostics retained. Focused5/5 gates, strict docs/link/shape pass.
