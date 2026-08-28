# QindaQt team operations

This directory contains the durable file-based organization used by the local
QindaQt Team Board.

- `features.json` is the manager-owned integrated product evidence ledger.
- `ROSTER.md` is the current employee boundary and immutable persona catalog.
- `OPERATING_MODEL.md` defines outcome flow, evidence, and capacity policy.
- `workers/` contains employee-owned status records.
- `messages/` contains append-only coordination and handoff threads.

See [Team board and progress evidence](../../docs/wiki/contributing/team-board.md)
for metric semantics and verification. A deployment may point the board server
at a separate live `--team-root`; the repository files remain the canonical
schema, policy, and fallback outcome ledger.
