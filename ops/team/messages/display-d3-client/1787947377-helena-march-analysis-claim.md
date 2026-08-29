# Helena March — Claim: Display1 D3 architecture and adversarial-analysis partner

- Time: 2026-08-28T20:03:00Z
- Provider/model: Anthropic Claude Code, `claude-fable-5`, reasoning high
- Role: analysis/planning partner for Pavel Shore (recovery implementer) and Tara Wells (test author). I implement nothing.
- Reference worktree (immutable): `/mnt/d/QindaQt/worktrees/display-d3-analysis-helena` at manager `0760e08`
- Read-only inspection target: `/home/cabewse/work_SPaC3/container-wm-workers/display-d3-kimi-nyra` (Nyra's preserved partial on base `146fc483`). I will not edit it, nor any product, test, CMake, ledger, queue, or another worker's profile.

## Bounded outcome

One durable, implementation-ready and review-ready analysis handoff in this thread covering:

1. Exact ownership/lifetime/threading/error/compatibility contracts for `DisplayTransport`, `QtDisplayTransport`, `Client`, and the still-unwritten `Coordinator`.
2. Owner/payload binding, real A/B/A owner lineage, coalesced `Changed` invalidation, and atomic last-known-good snapshot publication.
3. Cancellation/timeout/late-callback handling and exactly-once operation completion.
4. Preview→confirm/revert client-side deadline and service-loss state machine; transaction-token lineage across epochs.
5. Malformed/oversized payload handling at the QtDBus reply boundary.
6. Installed-header / source-policy / package seams.
7. Whether Tara's five test binaries genuinely prove each behavior, or which additional named rows are needed.

Deliverables: file/line findings against the current partial, smallest repair order, invariants and negative controls, tempting-but-incorrect fixes, and exact acceptance commands.

## What I will not do

No builds, background tasks, live buses, display/input, host configuration, or hardware. Findings are static-analysis evidence against the tree as observed, not compile/test evidence.

Pavel: if you land changes in the partial while I am analysing, reply here with the file list and I will re-inspect those files before the handoff.
