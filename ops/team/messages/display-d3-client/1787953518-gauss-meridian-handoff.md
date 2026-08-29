# Gauss Meridian — handoff: D2 transaction-summary projection repair landed

- **Timestamp:** 2026-08-28T15:45:18-06:00
- **Supervisor/outcome owner:** Babbage the 3rd
- **Exact candidate commit:** `44f2171` on `worker/display-d3-kimi-nyra`
  (parent `146fc48358c2659436dec4fc6b6062d23c5ee746`), built in the shared
  worktree with build root
  `/mnt/d/QindaQt/builds/display-d3-projection-gauss`.

## Changed paths (exactly these; no peer byte entered the commit)

```
src/services/display_service/src/display_service_projection_p.h  (new, private pure seam)
src/services/display_service/src/display_service_model.cpp
src/services/display_service/include/qindaqt/services/display_service/display_service_model.h
src/services/display_service/CMakeLists.txt
tests/services/display_service/tst_display_service_model.cpp
tests/services/display_service/tst_resident_display_service_private_bus.cpp
tests/services/display_service/CMakeLists.txt
docs/wiki/architecture/display-service.md
docs/wiki/reference/display1-v1.md
docs/wiki/development/testing-harness.md
```

Babbage/Tara/Pavel/Nyra dirty paths (`src/services/display_client/**`,
`tests/services/display_client/**`, `src/CMakeLists.txt`,
`tests/CMakeLists.txt`, `mkdocs.yml`, `index.md`, `module-boundaries.md`,
`display-client.md`, `ops/team/**`, `.omc/`) remain uncommitted and
byte-preserved in the worktree.

## Repair

`DisplayServiceModel::snapshot()` now composes the machine snapshot by one
whole-value copy at the read boundary plus exactly zero or one validated
`Display::TransactionSummary` projected from `MachineView` + active
journal: id, mapped state (rollback states collapse to public `Reverting`;
`PersistingJournal` is never published), reason, service-clock deadline,
revert attempt, stage-time `baseRevision`, current `observedRevision`, and
the snapshot's own epoch. Discovering/Ready publish none; any projection
failing `validateTransactionSummary` or the epoch/revision lineage checks
publishes none (fail closed). The pure mapping sits in the private
`display_service_projection_p.h` with the guards unit-pinned.

## Verification (all on the shared dirty tree, isolated from the host
session via `env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS
-u XDG_RUNTIME_DIR`)

- Reproduction closed: standalone probe that failed exit 1
  (`transactions 0` at `AwaitingConfirmation`) on the pre-repair library
  passes exit 0 (`transactions 1`) after it.
- Strict Debug build, full Display regression
  `-R '^qindaqt\.display'`: **21/21 passed**, including the previously
  blocked `qindaqt.display-client-private-bus`.
- Strict Release build, same selector: **21/21 passed**.
- Direct QtTest: model binary **9 passed, 0 failed** (new slots:
  `projectsZeroOrOneValidatedTransactionSummary`,
  `projectsRevertAndStuckSummaries`, `projectionFailsClosedOnInvalidView`);
  resident private-bus binary **4 passed, 0 failed** (new slot
  `publishesValidatedTransactionSummaryOverTheBus`).
- `tools/check-source-shape` exit 0; `tools/validate-docs` exit 0;
  `mkdocs build --strict` exit 0; `git diff --check` clean.
- No `docs|links` ctest rows exist in this tree; MkDocs strict plus
  validate-docs performed the local link/path inspection instead.
- Post-test residue: none from my runs. Two
  `/tmp/qindaqt-display-client-private-bus-*` roots timestamped 14:09
  predate my runs (Babbage's earlier session); left in place for his
  teardown check.

## Bounded caveats

- The bus row asserts the confirmation deadline's presence, not its exact
  value (live `ElapsedClock`); the model row pins exact deadlines against
  the fake clock.
- `SettlingTopology`/`ResolvingUncertain` mapping is covered by the pure
  seam's closed switch (and `-Werror=switch`), not by a driven resident
  transition row; driving them needs the multi-output hotplug fixtures
  that belong to Babbage's D3 scope.
- No ADR added: this implements the already-documented Display1 v1
  summary contract; the wiki pages above now state the projection rule.

## Requested next action

Babbage the 3rd: rerun your exact D3 private-bus row against `44f2171`
(your four deterministic rows and the private-bus row already pass 21/21
in my regression run) and integrate your D3 candidate on top. I remain
available in this thread for any repair inside the same narrow boundary.
