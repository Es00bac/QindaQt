# Pavel Shore — Resume claim: Display1 D3 client and reversible-transaction coordinator

Continuing as accountable recovery lead after a session interruption (Malik
Hart's routing/correction notes and my own preserved worker record confirm no
work was lost). Same worktree, branch, base commit as my original claim.

**Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/display-d3-kimi-nyra`
**Branch:** `worker/display-d3-kimi-nyra`
**Base commit:** `146fc48358c2659436dec4fc6b6062d23c5ee746`

## What's already landed (verified present on disk this session)

- `src/services/display_client/include/qindaqt/services/display_client/client.h`
  — renamed from Nyra's `display_client.h` to match what every one of Tara's
  five test files already includes; zero edits to Tara's files. Also fixed
  `PendingOperation::epochAtSubmit` from `quint64` to `QString` (it could
  never hold a real service epoch).
- `src/services/display_client/src/display_client.cpp` — full `Client`
  implementation: owner/epoch/revision-fenced snapshot and operation
  lineage, requestId-based rejection of stale/out-of-order/late transport
  replies, A/B/A owner-replacement handling, bounded fetch/operation
  deadlines with retry backoff, and exactly-once deferred completion
  delivery per operation.
- `src/services/display_client/include/.../display_coordinator.h` +
  `src/.../display_coordinator.cpp` — new `Coordinator`: a thin caller-facing
  layer (not a `Client` duplicate) owning single-transaction
  stage→preview→confirm|cancel sequencing, fail-closed gating on `Client`'s
  reported state/lineage, and a bounded client-side confirmation deadline
  that cancels (never commits) an undecided transaction.

## Tara Wells's work (read, not touched)

Her full claim/midpoint/implementation-note/handoff thread lives at
`ops/team/messages/display-d3-client-tests/` in this worktree (not the shared
board — noted for the record, matching Malik's correction). 5 hostile QtTest
binaries, 18 cases, support fixtures, and `tests/CMakeLists.txt` registration
are complete and were blocked only on the two files above. I have not edited
`tests/services/display_client/**`.

## Environment finding

The worktree's `build/` is a symlink to `/mnt/d/QindaQt/builds/display-d3-kimi-nyra`.
Building through that symlink breaks CMake AUTOMOC's generated relative
`#include` paths (a physical-vs-logical path depth mismatch once the symlink
is resolved). Building directly against the physical
`/mnt/d/QindaQt/builds/display-d3-kimi-nyra` path avoids it; using that path
directly for strict Debug/Release now.

**Next:** strict-warning Debug and Release builds, Tara's five hostile
binaries plus installed-consumer/source-shape checks, Display doc/ADR
updates, one clean commit, then a non-Claude exact review request.
