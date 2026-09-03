# Task list T1 handoff — production facts producer and operation adapter

- Worker: Irma Wyman (Moonshot Kimi `kimi-code/k3`, reasoning high), slug `irma-wyman`
- Candidate commit: `3a5ae1773cac99c0a5e67b1785cf79a9a262c8b4`
- Candidate tree: `3a79129bd56759afffe43dca0f7cc6f54c024e8b`
- Exact base: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9` (branch
  `worker/task-list-t1` = base + preserved `6a016c6` + candidate)
- Feature: QQ-004.10 Task list (T1 production facts producer and operation adapter)
- Requested next action: independent exact review of `3a5ae17` (which sits on
  the preserved `6a016c6`), then manager integration.

## Outcome delivered

1. `src/shell/task_list/producer/`: exact-owner asynchronous Compositor1
   client on an injected bus connection. One refresh joins `Windows()`,
   `Containers()`, and `ShellVisibilitySnapshot()` under the current unique
   owner; all three signals are invalidation hints only. A signal racing the
   three reads fences the whole refresh (discard, debounced re-read once).
   Late replies are fenced by (token, owner). Classification reuses the
   collapsed native identity (the single member with `skipTaskbar == false`
   becomes `ContainerPrimary`); zero/two native identities, unknown container
   owners, empty wire containers, and missing scope entries atomically reject
   the batch. Owner loss/replacement, malformed replies, source rejection,
   and timeouts publish `Degraded` with the last accepted generation retained,
   followed by a bounded retry backoff; no polling beyond debounce/retry.
2. `src/shell/task_list/operations/`: serialized exactly-once adapter over
   `Submit` (`activate-page`, `detach-window` with the accepted generation's
   container revision as `expectedRevision`), `ReleaseContainer`, and
   `DockWindows`. Admission is fenced atomically against owner, source status,
   generation revision (`StaleGeneration`), busy state (`Busy`), and container
   lineage (`UnknownContainer`, `UnsupportedAuthority` for `hybrid-process`).
   Timeouts, malformed replies, bus failures after send, and owner change in
   flight finish `Uncertain` and are never resubmitted. Window-level
   activate/minimize/close and container activate/minimize/close finish
   `Unavailable` with exact `compositor-window-*-unavailable` /
   `compositor-container-*-unavailable` codes; no private path was invented.
3. Tests under `qindaqt.task-list-` (13 rows): hostile wire decoding,
   joiner classification negatives, producer owner lineage and fencing, adapter
   admission fencing, a new `qindaqt.task-list-operation-results` row proving
   reply mapping and exactly-once lineage, a private-bus Qt transport row with
   a fake `org.qindaqt.Compositor1` service (fresh `dbus-daemon` per test,
   owner loss/replacement, stale-owner refusal), and the source-boundary
   poison check. No host bus, compositor, display, input, or network contact;
   leaked test daemons from the interrupted run were reaped, and the current
   tests leave none behind.
4. Docs: `docs/wiki/shell/task-list.md` production sections, additive rows in
   `docs/wiki/architecture/module-boundaries.md` and
   `docs/wiki/development/testing-harness.md`. No new wiki page, so no
   `mkdocs.yml` edit; no ADR — ADR-0044's boundary is unchanged.

## Repairs applied on resume (candidate `3a5ae17` on top of preserved `6a016c6`)

- `tests/shell/task_list/task_list_operation_test_support.h` missed the
  `TaskListOperationTransport` include (incomplete base class), and its
  Q_OBJECT fakes needed listing as target sources for AUTOMOC.
- The testing-harness page claimed Submit/Release/Dock reply-mapping and
  exactly-once Uncertain coverage that had no test; added
  `tst_task_list_operation_results.cpp` (9 cases) and corrected the row count.

## ADR-0063 compositor identity note

`main` integrated the authenticated `CompositorShell1` identity snapshot after
my base. The T1 producer does not need it: window identity for task-list facts
comes from the public Compositor1 `Windows()` read itself, and window-level
operations are `Unavailable` by design, so no window-action client is
consumed. `main` was not merged; the candidate applies additively.

## Changed paths (base..candidate, sorted)

    docs/wiki/architecture/module-boundaries.md
    docs/wiki/development/testing-harness.md
    docs/wiki/shell/task-list.md
    src/shell/task_list/CMakeLists.txt
    src/shell/task_list/operations/CMakeLists.txt
    src/shell/task_list/operations/include/qindaqt/shell/task_list/operations/qt_task_list_operation_transport.h
    src/shell/task_list/operations/include/qindaqt/shell/task_list/operations/task_list_operation_adapter.h
    src/shell/task_list/operations/include/qindaqt/shell/task_list/operations/task_list_operations.h
    src/shell/task_list/operations/include/qindaqt/shell/task_list/operations/task_list_operation_transport.h
    src/shell/task_list/operations/src/qt_task_list_operation_transport.cpp
    src/shell/task_list/operations/src/task_list_operation_adapter.cpp
    src/shell/task_list/producer/CMakeLists.txt
    src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/qt_task_list_producer_transport.h
    src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_fact_joiner.h
    src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_facts_producer.h
    src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_producer_transport.h
    src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_wire.h
    src/shell/task_list/producer/src/qt_task_list_producer_transport.cpp
    src/shell/task_list/producer/src/task_list_fact_joiner.cpp
    src/shell/task_list/producer/src/task_list_facts_producer.cpp
    src/shell/task_list/producer/src/task_list_wire.cpp
    tests/shell/task_list/CMakeLists.txt
    tests/shell/task_list/check_task_list_boundary.cmake
    tests/shell/task_list/task_list_operation_test_support.h
    tests/shell/task_list/task_list_producer_test_support.h
    tests/shell/task_list/tst_task_list_fact_joiner.cpp
    tests/shell/task_list/tst_task_list_facts_producer.cpp
    tests/shell/task_list/tst_task_list_operation_adapter.cpp
    tests/shell/task_list/tst_task_list_operation_results.cpp
    tests/shell/task_list/tst_task_list_qt_transports.cpp
    tests/shell/task_list/tst_task_list_wire.cpp

## Evidence (every command actually run by this worker)

Build root `<ROOT>` = `/home/cabewse/work_SPaC3/builds/qindaqt/task-list-t1`;
both trees were already configured with the prescribed recipe by the
interrupted run (configure logs retained; Debug `CMakeCache.txt` confirms
`CMAKE_BUILD_TYPE=Debug`, strict warnings, testing on).

- Debug build: `cmake --build <ROOT>/debug --parallel 3 --target
  qindaqt_shell_task_list qindaqt_shell_task_list_producer
  qindaqt_shell_task_list_operations <all 11 task-list test targets>` —
  exit 0, strict warnings clean.
- Debug tests: `ctest --test-dir <ROOT>/debug -R '^qindaqt\.task-list-'
  --output-on-failure --no-tests=error` — exit 0, 13/13 passed.
- Release build: same target list against `<ROOT>/release` — exit 0, strict
  warnings clean.
- Release tests: same ctest selector against `<ROOT>/release` — exit 0,
  13/13 passed.
- `./tools/validate-docs` — exit 0 (117 documents + mkdocs.yml navigation).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build
  --strict --site-dir <ROOT>/site` — exit 0.
- `./tools/check-source-shape` — exit 0 (two pre-existing
  decomposition-review warnings in other lanes' files; none in task-list
  paths).
- `git diff --check` — exit 0.
- No JSON files changed; `json.tool` gate not applicable.
- Post-test process check: zero `--print-address=1` private daemons left by
  these test runs (15 stale daemons from the interrupted Sep 2 run reaped).

## Remaining bounded caveats (deliberately not claimed)

- The producer and adapter are compiled and tested but **not instantiated by
  the production shell**; shell composition, QML presentation, and installed
  keyboard/accessibility qualification are later lanes.
- Window-level activate/minimize/close and whole-container activate/minimize/
  close are `Unavailable`; the exact Compositor1 extensions needed are
  recorded in `docs/wiki/shell/task-list.md` ("Protocol extension requests")
  as a later compositor lane.
- `urgent` is published `false` and `applicationName == applicationId` until
  the protocol grows those fields (same wiki section).
- `Submit`/`ReleaseContainer` mutations cover `control-bridge` containers
  only; `hybrid-process` mutation authority is a compositor-lane decision.
- No runtime, hardware, or nested-session evidence is claimed; all transport
  proof is fake-service-on-private-bus only.
