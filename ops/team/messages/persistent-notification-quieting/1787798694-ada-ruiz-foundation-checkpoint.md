# Foundation checkpoint — Ada Ruiz

- Timestamp: 2026-08-27T02:44:54Z
- Branch/worktree: `worker/ada-settings1` at the clean assigned base

The independently ported/adapted schema, migration, protocol, and resident
service foundation now compiles with strict Debug warnings and passes 7/7
focused tests:

```text
qindaqt.settings-schema
qindaqt.settings-layers
qindaqt.settings-persistence
qindaqt.settings-migration
qindaqt.settings-protocol
qindaqt.settings-repository
qindaqt.settings-service-lifecycle
```

Material correction beyond Mira's paused partial work: the Settings1 value
codec now supports recursively nested JSON-native null/boolean/integer/finite
number/string/array/object shapes, with explicit per-container count, UTF-8,
aggregate byte, node, depth, and transaction aggregate bounds. The flat-map
restriction and its incorrect generic-Object claim were not adopted.

The resident service is an independent activation-ready executable and uses
candidate mutation, candidate atomic save, then authoritative swap/publication;
the private-bus lifecycle test proves distinct-connection name collision,
release, and replacement ownership. The migration/default/corruption and
repository no-op/conflict/save-failure/revision-exhaustion boundaries are
covered in focused tests.

No blocker. Remaining: exact-owner async client and real transport scenarios;
DND controller/shell bridge and privacy precedence; standalone settings app,
quick toggle and fixed launcher UI; restart/UI/install tests; docs/ADR and full
Debug/Release/production validation.
