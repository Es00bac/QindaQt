# QindaQt team operations

This directory contains the durable file-based organization used by the local
QindaQt Team Board.

- `features.json` is the manager-owned integrated product evidence ledger.
- `ROSTER.md` catalogs stable core personas and staffing intent; it does not
  hide other durable employee records from the board.
- `OPERATING_MODEL.md` defines outcome flow, evidence, and capacity policy.
- `queues/` contains the Shell, Platform, and First-party delivery queues.
- `workers/` contains employee-owned status records.
- `messages/` contains append-only coordination and handoff threads.

See [Team board and progress evidence](../../docs/wiki/contributing/team-board.md)
for metric semantics and verification. A deployment may point the board server
at a separate live `--team-root`; the repository files remain the canonical
schema, policy, and fallback outcome ledger.
