# Nyra Sol — Claim: Display1 D3 client and reversible-transaction coordinator

**User-visible outcome:** Implement the next cohesive Display1 D3 vertical slice: an asynchronous Qt client for the already integrated resident `org.qindaqt.Display1` service plus a fail-closed reversible preview/confirm/revert coordinator that consumes the existing public protocol and transaction types.

**Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/display-d3-kimi-nyra`
**Branch:** `worker/display-d3-kimi-nyra`
**Base commit:** `146fc48358c2659436dec4fc6b6062d23c5ee746`
**Provider/model:** Moonshot Kimi Code managed coding route, `kimi-code/kimi-for-coding`, reasoning high.

**Owned paths (new):**
- `src/services/display_client/**`
- `tests/services/display_client/**`
- Smallest additive top-level CMake registration in `src/services/CMakeLists.txt`
- Owning Display wiki/ADR/testing-harness updates (`docs/wiki/architecture/display-service.md`, `docs/wiki/reference/display1-v1.md`, ADR-0016 consequences if needed, `docs/wiki/development/testing-harness.md`)

**Explicitly not owned / will not edit:**
- `docs/TASK_LIST.md`, `docs/HANDOFF.md`, `ops/team/features.json`
- Existing Display protocol/service/topology/transaction implementations
- Other services/apps, integration branches, or unrelated worker paths

**Acceptance target:**
- Strict-warning Debug and Release builds
- Hostile fake-transport tests for owner A/B/A, revision/epoch rollback, timeout, late reply, atomic publication, preview/confirm/revert/cancel, teardown, and installed public-header consumer
- All direct QtTest totals recorded
- source-shape, docs/link, strict MkDocs, whitespace, clean tree
- One clean milestone candidate commit, then request a non-Kimi exact review

**Collision/dependency risks:** None identified. The D2 resident service and exact-owner adapter are already integrated at base; this slice adds only a new consumer/coordinator boundary above them. No concurrent Display worker is active on the board.
