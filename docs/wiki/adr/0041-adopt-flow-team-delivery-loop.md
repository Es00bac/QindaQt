# ADR-0041: Adopt the Flow team delivery loop

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** QindaQt Program Management
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt already had truthful product metrics, isolated worktrees, and exact
candidate review, but completed candidates could remain parked, finished
workers could become passive, and a manually maintained roster could hide
genuine live employee records. Those gaps reduced integration throughput even
when many independent product lanes existed.

Direct inspection of a completed sibling project's repository showed a
self-propelled loop: stable personas owned whole outcomes, peers exchanged
bounded evidence, exact candidate failures returned immediately to the same
implementer/reviewer pair, accepted commits integrated promptly, and finished
people scanned the queue for the next outcome or help request. Its activity
volume was evidence of this loop, not the product metric itself.

## Decision

QindaQt adopts that delivery loop with three stable workgroup queues for Shell,
Platform, and First-party work. Workgroup managers own dispatch, help routing,
and queue freshness. The Program Manager remains the only integrator and the
only owner of the product evidence ledger.

Every durable employee record is visible to the board. `ROSTER.md` is a core
organization catalog, never a visibility allowlist. Liveness remains fail-
closed on a self-owned fresh declaration plus direct process evidence, and the
Program Manager enforces the 15-live-process ceiling.

An implementer hands off one immutable candidate; an independent reviewer
attacks that commit; real defects return to the implementer; the same reviewer
checks the repaired descendant; and a passing candidate integrates promptly.
After handoff or review, workers scan their queue and peer threads and either
claim compatible work or offer concrete help.

## Consequences

- Product score remains based only on accepted integrated behavior.
- Durable queues add an explicit next-action and ownership index without
  replacing detailed message threads or the canonical outcome ledger.
- Stable workgroup managers reduce Program Manager dispatch bottlenecks but do
  not gain integration or evidence authority.
- The board must test that stale roster content cannot hide valid worker
  records.
- The Program Manager must actively arbitrate shared registries, compilers,
  nested sessions, buses, input fixtures, and hardware so capacity refill does
  not create collisions.
- Useful branches, reviews, and handoffs remain preserved when staffing or
  direction changes.

## Revisit when

Measured integration latency or defect escape rates show that the workgroup
queue and refill loop costs more delivery time than it saves, or repository
scale requires a different durable dispatch boundary.
