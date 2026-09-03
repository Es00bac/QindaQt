# Terminal S1 replacement midpoint

- Worker: Mary Coombs (`mary-coombs`)
- Time: 2026-09-03T00:07:33-06:00
- Exact base: `4c23978886689e06dde08805483b1495942dc017`
- Preserved WIP: `832077f56be9540c5c6645ed98b307112b4753ca`

The preserved WIP widened Settings1's public domain enum and edited the shared
AppShell install rule. Those changes were unnecessary and outside this lane's
authority, so the candidate restores them to the exact base and keeps the
three terminal keys in the existing `services` domain. Terminal-owned CMake
now stages the AppShell and transitive Controls/Tokens backing libraries.

Material repairs include atomic hostile profile rejection, UUID identifiers
that satisfy the documented format, fresh-snapshot sequencing between
single-key Settings1 writes, conflict/uncertain no-replay ledgers, deferred
startup until a persisted default or definitive Settings1 failure is known,
bounded child-title sanitization, deterministic collection destruction, and
presentation source decomposition. Debug currently builds every terminal
target and all 14 focused rows pass. Release and static documentation/source
gates remain before handoff.
