# Vivienne Malone-Mayes-Codex — task list T1 repair handoff

- Candidate commit: `7b6bd8ac74511a0cbbe6fb4088305655ad047340`
- Candidate tree: `de677c98c5b8bbc9f05274ed6d559cd226b96611`
- Repair parent: `50c21626202f9ec876f2490e74cb414d2b207620`
- Rejected candidate: `3a5ae1773cac99c0a5e67b1785cf79a9a262c8b4`
- Exact lane base: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`
- Branch: `worker/task-list-t1`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/task-list-t1`
- Requested next action: independent exact review, then manager integration.

## Finding closure

- **P1-1 — torn/foreign scope truth:** closed by `7b6bd8ac`. The producer
  consumes only the exact-owner schema-2 `Windows()` inventory and never calls
  or combines `ShellVisibilitySnapshot()` or `Containers()`. It validates UUID
  epoch, nonzero available generation revision, owner/token/invalidation race,
  revision regression, equal-revision collision, and same-owner foreign epoch.
  Because public Compositor1 has no coherent T0 inventory, it publishes
  Degraded/unavailable truth instead of placeholders. Registered controls:
  `qindaqt.task-list-facts-producer::documentedWindowInventoryCannotPublishTornFacts`,
  `::foreignEpochRegressionAndCollisionAreRejected`,
  `qindaqt.task-list-qt-transports::producerTransportBindsReadsAndSignalsToTheExactOwner`,
  and `qindaqt.task-list-boundary`.
- **P1-2 — silent failure/live state after stop:** closed by `7b6bd8ac`.
  `failRefresh()` and every owner/start/stop failure transition mark the source
  Degraded, update error/availability truth, and emit `stateChanged`; stop also
  clears the bound owner and window lineage. Registered controls:
  `qindaqt.task-list-facts-producer::failedRefreshRetainsGenerationAndSignals`,
  `::malformedInventoryDegradesAndSignals`,
  `::stopWithdrawsAvailabilityAndSignals`, and
  `qindaqt.task-list-operation-adapter::stoppedProducerRejectsBeforeBusTraffic`.
- **P1-3 — forged/recycled operation replies:** closed by `7b6bd8ac`.
  `Submit` settles only on canonical `protocol`, `transactionId`,
  `containerId`, `status`, and `revision`, with committed revision exactly one
  beyond the fenced container revision. Tokens are monotonic at the lifetime
  of the transport that owns pending calls, so adapter reconstruction cannot
  recycle an old watcher lineage. Registered controls in
  `qindaqt.task-list-operation-lineage` cover missing echoes, forged protocol,
  transaction/container echoes, malformed revisions, incorrect committed
  revision, and cross-adapter late replies.
- **P2-1 — missing hostile/stress controls:** closed by `7b6bd8ac`.
  `qindaqt.task-list-wire::windowsInventoryBoundIsExact` exercises 4,096 and
  4,097 windows; the wire row also covers malformed, oversized, duplicate,
  invalid-text, schema, epoch, revision, and container inventories. The facts
  row adds the 4,096-window producer path and a non-replying authority with
  observable timeout plus bounded retry.
- **P3-1 — stale window-operation direction:** closed by `7b6bd8ac` in
  `docs/wiki/shell/task-list.md`, aligned to ADR-0061, ADR-0063, and the existing
  exact-owner `src/shell_window_actions_client`. `qindaqt.task-list-boundary`
  requires those markers so the page cannot silently regress to requesting a
  duplicate Compositor1 action/identity reader.

The original reviewer reproduction binaries were run before repair under the
required hostile bus environment. Both exited 0 and reproduced the rejection:

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  /home/cabewse/work_SPaC3/builds/qindaqt/lanes/review-tasklist-codex/repro/repro-dev
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  /home/cabewse/work_SPaC3/builds/qindaqt/lanes/review-tasklist-codex/repro/repro-release
```

Observed in both profiles: foreign scope reached Ready, degradation emitted no
signal, post-stop mutation reached the transport, a status-only Submit reply
committed, and reconstructed adapters both allocated token 1. The repaired
registered controls above replace those unsafe paths and pass in both profiles.

## Changed paths (candidate commit, sorted)

```text
docs/wiki/architecture/module-boundaries.md
docs/wiki/development/testing-harness.md
docs/wiki/shell/task-list.md
src/shell/task_list/include/qindaqt/shell/task_list/task_list_source.h
src/shell/task_list/operations/include/qindaqt/shell/task_list/operations/qt_task_list_operation_transport.h
src/shell/task_list/operations/include/qindaqt/shell/task_list/operations/task_list_operation_adapter.h
src/shell/task_list/operations/include/qindaqt/shell/task_list/operations/task_list_operation_transport.h
src/shell/task_list/operations/src/qt_task_list_operation_transport.cpp
src/shell/task_list/operations/src/task_list_operation_adapter.cpp
src/shell/task_list/operations/src/task_list_operation_reply.cpp
src/shell/task_list/producer/CMakeLists.txt
src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/qt_task_list_producer_transport.h
src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_fact_joiner.h (deleted)
src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_facts_producer.h
src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_operation_authority.h (added)
src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_producer_transport.h
src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_wire.h
src/shell/task_list/producer/src/qt_task_list_producer_transport.cpp
src/shell/task_list/producer/src/task_list_fact_joiner.cpp (deleted)
src/shell/task_list/producer/src/task_list_facts_producer.cpp
src/shell/task_list/producer/src/task_list_scope_wire.cpp (deleted)
src/shell/task_list/producer/src/task_list_wire.cpp
src/shell/task_list/producer/src/task_list_wire_detail.h
src/shell/task_list/src/task_list_source.cpp
tests/shell/task_list/CMakeLists.txt
tests/shell/task_list/check_task_list_boundary.cmake
tests/shell/task_list/task_list_operation_test_support.h
tests/shell/task_list/task_list_producer_test_support.h
tests/shell/task_list/tst_task_list_fact_joiner.cpp (deleted)
tests/shell/task_list/tst_task_list_facts_producer.cpp
tests/shell/task_list/tst_task_list_intents.cpp
tests/shell/task_list/tst_task_list_operation_adapter.cpp
tests/shell/task_list/tst_task_list_operation_lineage.cpp
tests/shell/task_list/tst_task_list_operation_results.cpp
tests/shell/task_list/tst_task_list_qt_transports.cpp
tests/shell/task_list/tst_task_list_wire.cpp
```

## Verification evidence

Debug configure (exit 0):

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/task-list-t1/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Release configure used the same command with `release` and
`-DCMAKE_BUILD_TYPE=Release` (exit 0). Both generated against the prescribed
KWin 6.6.5 cache; CMake emitted the repository's existing mixed-prefix RPATH
warnings.

Final strict focused builds ran in both profiles (exit 0, 15 named targets):

```sh
TMPDIR=<ROOT>/<profile>/tmp cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_shell_task_list qindaqt_shell_task_list_producer \
  qindaqt_shell_task_list_operations qindaqt_task_list_values_tests \
  qindaqt_task_list_source_grouping_tests \
  qindaqt_task_list_source_validation_tests qindaqt_task_list_intents_tests \
  qindaqt_task_list_scope_filter_tests qindaqt_task_list_presentation_tests \
  qindaqt_task_list_wire_tests qindaqt_task_list_facts_producer_tests \
  qindaqt_task_list_operation_adapter_tests \
  qindaqt_task_list_operation_results_tests \
  qindaqt_task_list_operation_lineage_tests qindaqt_task_list_qt_transports_tests
```

Focused tests (both exit 0, 13/13 passed):

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <ROOT>/debug -R '^qindaqt\.task-list-' \
  --output-on-failure --no-tests=error
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <ROOT>/release -R '^qindaqt\.task-list-' \
  --output-on-failure --no-tests=error
```

The final documentation-only adjustment was followed by the boundary row in
both profiles (exit 0, 1/1 each). Process snapshots before/after every final
full test run found no newly retained private `dbus-daemon`; none required
killing.

Static gates:

```sh
./tools/validate-docs
# exit 0: 129 Markdown documents and mkdocs.yml navigation validated
TMPDIR=<ROOT>/mkdocs-tmp \
  /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir <ROOT>/site
# exit 0
./tools/check-source-shape
# exit 0: 2,118 files checked; four pre-existing warnings outside owned paths
git diff --check
git diff --check 50c21626202f9ec876f2490e74cb414d2b207620
# both exit 0
```

No JSON changed, so the JSON syntax gate is not applicable. An additional
`clang-format --dry-run --Werror` diagnostic exited 1 because this repository
has no `.clang-format` and the tool's fallback LLVM style disagrees with both
existing and repaired task-list sources; clang-format is not a repository gate.

## Bounded caveats

- This candidate deliberately cannot publish a Ready task-list generation
  from Compositor1 1.1. A single atomic inventory with T0 identity, scope,
  state, and container lineage is the required compositor prerequisite.
- Mutation behavior is tested through the narrow injected operation authority;
  the production fail-closed producer exposes no container lineage. This avoids
  fabricating a generation solely to exercise mutations.
- The five-field canonical reply echo applies to `Submit`. Successful
  `DockWindows` uses compositor-generated ids and is checked for protocol/id/
  revision shape plus exact pending `(token, owner)`; `ReleaseContainer` carries
  only status/failure on the current wire and is bound by the same exact pending
  call lineage.
- No production-shell instantiation, QML rendering, nested compositor, host
  session bus, hardware, uinput, or network coverage is claimed or was run.

Requested next action: **independent exact review, then manager integration**.
