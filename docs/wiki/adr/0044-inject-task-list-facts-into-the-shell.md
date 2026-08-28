# ADR-0044: Inject task-list facts into the shell

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Shell task list

## Context

The shell must present one task row per window and per collapsed QindaQt
container, accept activation/minimize/close requests, and offer keyboard and
accessibility identities. Window truth lives in the compositor and the
process-local Hybrid runtime, and the shell never links KWin private objects
([Module boundaries](../architecture/module-boundaries.md)). The bounded
Task-list T0 slice therefore needs a dependency direction that cannot drift
into window management or compositor internals.

## Decision

The shell task list is a pure injected-facts model. Shell composition copies
one coherent generation of immutable window facts — already classified with
standalone, container-primary, and container-member roles — into
`src/shell/task_list`. The module validates the batch atomically, collapses
containers to their primary identity, orders rows deterministically, filters
per output/workspace, projects loading/empty/degraded presentation, and
arbitrates typed request intents with stale-id rejection. It never performs a
window operation: executing accepted intents stays with a separate shell
adapter, and producing facts stays with a separate compositor/hybrid snapshot
producer.

## Consequences

- The task list cannot bypass compositor authority, spawn window operations,
  or depend on KWin ABI; hostile facts are rejected without replacing the
  retained generation.
- Container collapse policy is enforced twice by design: the producer
  classifies roles, and the model re-validates primary/member pairing, so a
  misprojected container fails loudly instead of showing duplicate rows.
- Keyboard order equals the canonical entry order, making traversal stable
  and testable without a running shell.
- Presentation and intent policy are verifiable as pure values; runtime,
  transport, and installed accessibility qualification remain later slices
  and must not be claimed by this boundary.
- A future facts producer must keep identity spaces disjoint (window ids
  versus container ids) because task identities address rows.

## Revisit when

A live compositor/hybrid producer exists and a different classification point
(on the compositor side of the wire, for example) would remove duplication
without giving the shell private compositor dependencies; or when persisted
task-list state (pinning, grouping overrides) requires a schema.
