# ADR-0102: Adopt restored layouts atomically

- Status: Accepted
- Date: 2026-09-07

## Context

Reopening a saved workspace can involve several pages and splits. Replaying
individual dock operations would expose intermediate groups and leave partial
ownership if a later window disappeared or scene operation failed.

## Decision

Add `AdoptIndependentLayout` to the process-local Hybrid command set. It carries
one owned Core container value whose leaves already reference live window IDs.
The command requires at least two members, a fresh container ID, a valid Core
layout and all members currently independent. It edits only the coordinator's
private candidate, then uses the existing scene prepare/commit/rollback path.
One successful scene commit publishes one topology revision.

Keep application intent, persisted documents and launch outside Hybrid. The
compositor adapter supplies live eligibility and presentation identity; the
workspace policy supplies the bound layout. This is not a new external D-Bus
mutation method and does not change compatibility bridge ownership.

## Consequences

A failed adoption leaves existing ownership and revision unchanged. No sequence
of partially restored groups becomes observable. Stale selections are reported
to the assignment UI so the user can refresh and choose replacements. Pure
command tests prove the transaction contract; installed-session restoration
still requires the native UI/compositor adapter and nested runtime evidence.

See [Hybrid topology](../architecture/hybrid-topology.md).
