# Exact-candidate recheck — Grete Hermann

- Persona: Grete Hermann
- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Candidate SHA: `7b6bd8ac74511a0cbbe6fb4088305655ad047340`
- Tree SHA: `de677c98c5b8bbc9f05274ed6d559cd226b96611`
- Parent SHA: `50c21626202f9ec876f2490e74cb414d2b207620`
- Product base SHA: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`
- Product base tree SHA: `4f2ceebbe8c28145c6e6e6cfd602d67bbaf33d11`
- Rejected ancestor: `3a5ae17`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/task-list-t1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex`

## Findings ledger

### P0

None.

### P1

#### P1-1 — An impossible `DockWindows` revision is classified as committed

The public protocol fixes every successful `DockWindows` result at revision
`"1"` (`docs/wiki/reference/compositor-control-v1.md:46,55-61`), and the
compositor constructs the staged split with expected revision zero before
returning the bridge's first committed revision
(`src/compositor/kwin/kwincontrolendpoint.cpp:392-407`). The task-list reply
classifier instead accepts any canonical unsigned-decimal string: the shared
parser admits zero (`src/shell/task_list/operations/src/task_list_operation_reply.cpp:29-41`),
and the Dock path checks only parseability before returning `Committed`
(`src/shell/task_list/operations/src/task_list_operation_reply.cpp:142-157`).

Reproduction source:
`/home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/repro/r2_probe.cpp`.
It classifies this reply for a Dock expectation:

```json
{"protocol":{"major":1,"minor":1},"transactionId":"dock-x","containerId":"container-x","status":"docked","revision":"0"}
```

The probe was linked separately to each candidate profile and run:

```text
Debug:   dock_zero_revision verdict=0
Release: dock_zero_revision verdict=0
```

`TaskListReplyVerdict::Committed` is enum value zero
(`src/shell/task_list/operations/include/qindaqt/shell/task_list/operations/task_list_operation_reply.h:28-40`).
Observed: the impossible reply settles as committed. Expected: it must settle
fail-closed as `UncertainLineage`, just as another reply that cannot be the
canonical outcome of the pending transaction. The only registered Dock result
test supplies revision `"1"` (`tests/shell/task_list/tst_task_list_operation_results.cpp:227-251`),
so it has no negative control for this malformed/out-of-contract reply.

This is P1 because the adapter reports successful mutation from a reply that
the compositor contract cannot produce.

### P2

#### P2-1 — Cold start with no compositor owner remains silently `Loading`

On a private session bus where `org.qindaqt.Compositor` has no owner,
`GetNameOwner` completes with an error and calls `bindOwner({})`
(`src/shell/task_list/producer/src/qt_task_list_producer_transport.cpp:132-158`).
Because the transport's initial owner is already empty, `bindOwner` immediately
returns without publishing `serviceOwnerChanged({})`
(`src/shell/task_list/producer/src/qt_task_list_producer_transport.cpp:161-164`).
The producer therefore never calls `markDegraded()` and never emits
`stateChanged`.

The same scratch probe starts a private `dbus-daemon`, deliberately registers
no compositor service, starts the real Qt transport/producer, and waits one
second. Both profiles produced:

```text
initial_unowned started=1 status=0 owner_empty=1 signals=0 error=
```

`TaskListSourceStatus::Loading` is enum value zero
(`src/shell/task_list/include/qindaqt/shell/task_list/task_list_types.h:143-150`).
Observed: known absence remains indistinguishable from an outstanding read,
with no error and no observer notification. Expected: `Degraded`, a nonempty
unavailability reason, and one `stateChanged`; the public source contract says
Degraded distinguishes a known unavailable authority before the first
generation (`src/shell/task_list/include/qindaqt/shell/task_list/task_list_source.h:30-34`),
and the task-list wiki says a failed first refresh is Degraded
(`docs/wiki/shell/task-list.md:69-75`).

The registered owner-loss test begins with a nonempty owner and therefore does
not exercise this initial empty-to-empty resolution
(`tests/shell/task_list/tst_task_list_facts_producer.cpp:185-208`). The bounded
workaround is for the compositor service to appear later, so this is P2 rather
than P1.

### P3

None.

## Recheck of the prior ledger

- Prior P1-1 (torn/foreign facts): closed. The architectural rebuttal is
  accepted: Compositor1 1.1 has no coherent task-list inventory, so the producer
  reads only schema-2 `Windows()` and deliberately publishes no generation or
  container lineage. `documentedWindowInventoryCannotPublishTornFacts` asserts
  Degraded, revision zero, no container lineage, a coherent-inventory error, and
  observer notification (`tests/shell/task_list/tst_task_list_facts_producer.cpp:88-104`).
  Foreign epoch, regressed revision, equal-revision collision, invalidation
  races, and old-owner replies are also registered negative controls.
- Prior P1-2 (`stateChanged` and stop): closed. A failed refresh must add exactly
  one notification while retaining the generation
  (`tests/shell/task_list/tst_task_list_facts_producer.cpp:109-128`); stop must
  degrade, clear the owner/in-flight state, and notify
  (`tests/shell/task_list/tst_task_list_facts_producer.cpp:256-269`). Operation
  admission after a stopped producer is separately registered.
- Prior P1-3 (canonical Submit lineage and lifetime fencing): closed. Missing
  echoes, forged container/transaction/protocol, non-success revision
  requirements, the exact `expected + 1` commit revision, and adapter
  reconstruction with a transport-lifetime token are explicit assertions in
  `tests/shell/task_list/tst_task_list_operation_lineage.cpp:30-205`.
- Prior P2-1 (registered stress/negative controls): closed as a coverage repair.
  The suite registers a nonreply timeout/retry assertion, the exact 4,096/4,097
  boundary (`tests/shell/task_list/tst_task_list_wire.cpp:129-139`), hostile
  identity text, and an at-limit producer path. The absence of a registered
  control was the prior defect; no deliberate product mutation is required to
  make the already-correct 4,096/4,097 behavior fail.
- Prior P3-1 (window-operation documentation): closed. The refreshed section
  identifies the existing authenticated shared client, limits this adapter to
  temporary `Unavailable`, and forbids a second identity/action reader
  (`docs/wiki/shell/task-list.md:161-187`).

The historical negative-control commit `5b93db04` changes only tests relative
to task-list product sources byte-identical to `3a5ae17`. Running its four
focused suites produced 12 assertion failures: wire 3, facts producer 4,
operation adapter 1, operation results 4. This directly verifies that the
original hostile assertions were not tautological. The repaired candidate's
replacement/retained registered tests pass in both profiles.

## Regression and boundary checks

- The scratch probe confirms title and application ID lengths of 512 decode
  successfully and 513 fail as `InvalidWindow` in both profiles:
  `wire_bounds title512=0 title513=6 app512=0 app513=6`.
- Hot owner loss clears the owner and degrades the source; replacement fences an
  old-owner reply. The current registered assertions pass in both profiles.
- The producer issues only `Windows()`/observes `WindowsChanged`; the Qt
  transport test verifies zero `Containers()` and
  `ShellVisibilitySnapshot()` calls. A source search found no task-list
  producer construction outside its owning module/tests, so the repair does not
  introduce a second task-list compositor client.
- The merge-conflict resolutions in `9b1088ed` preserve the canonical shared
  window-actions-client lineage and the revised independent-inventory rule.

## Commands and results

### Identity and cleanliness

```sh
git rev-parse HEAD
git show -s --format='candidate=%H%ntree=%T%nparents=%P' HEAD
git show -s --format='base=%H%ntree=%T%nparents=%P' f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9
git status --porcelain
```

Result: exit 0; candidate, tree, parent, and base match the header. Status was
empty before review and after all review work.

Repair/merge inspection included:

```sh
git diff --stat 3a5ae17..7b6bd8a
git diff --stat 50c2162..7b6bd8a
git diff 3a5ae17..7b6bd8a -- src/shell/task_list tests/shell/task_list docs/wiki/shell/task-list.md docs/wiki/architecture/module-boundaries.md
git show --cc 9b1088ed
```

Result: exit 0. The repair-specific parent diff contains 36 task-list/docs
paths; the broad ancestor diff contains the intervening merge from `main`, whose
two conflict resolutions were reviewed.

### Configure and focused build

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both configure commands: exit 0.

For each profile, the following exact target set was built with `--parallel 3`:

```sh
cmake --build <PROFILE_BUILD> --parallel 3 --target \
  qindaqt_shell_task_list qindaqt_shell_task_list_producer \
  qindaqt_shell_task_list_operations qindaqt_task_list_values_tests \
  qindaqt_task_list_source_grouping_tests qindaqt_task_list_source_validation_tests \
  qindaqt_task_list_intents_tests qindaqt_task_list_scope_filter_tests \
  qindaqt_task_list_presentation_tests qindaqt_task_list_wire_tests \
  qindaqt_task_list_facts_producer_tests qindaqt_task_list_operation_adapter_tests \
  qindaqt_task_list_operation_results_tests qindaqt_task_list_operation_lineage_tests \
  qindaqt_task_list_qt_transports_tests
```

`<PROFILE_BUILD>` was exactly the Debug and Release build directory above.
Both builds: exit 0; final incremental confirmation reported `ninja: no work to
do`. A preliminary Release invocation was accidentally overlapped by the review
harness and Ninja recovered its build metadata; the subsequent single required
build completed all 74 pending edges and the final confirmation passed. This was
confined to the assigned build root.

### Candidate tests

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/debug \
  -R '^qindaqt\.task-list-' --output-on-failure --no-tests=error

env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/release \
  -R '^qindaqt\.task-list-' --output-on-failure --no-tests=error
```

- Debug: exit 0, 13/13 passed, 0 failed, 1.67 s.
- Release: exit 0, 13/13 passed, 0 failed, 1.59 s.

Private-bus cleanup was audited by snapshotting exact `dbus-daemon` PIDs around
an additional focused transport row:

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/debug \
  -R '^qindaqt\.task-list-qt-transports$' --output-on-failure --no-tests=error
```

Result: exit 0, 1/1 passed. Before count 46, after count 46, no new PID; there
was no row-owned daemon to kill. Pre-existing daemons were not touched. No
`tests/session`, nested compositor, host bus, hardware, uinput, or network row
was run.

### Historical negative controls

```sh
git diff --quiet 3a5ae17 5b93db04 -- src/shell/task_list
git archive --format=tar 5b93db04 | tar -x -C /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/negative-5b93-src
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/negative-5b93-src/tests/shell/task_list \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/negative-5b93-debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/negative-5b93-debug \
  --parallel 3 --target qindaqt_task_list_wire_tests \
  qindaqt_task_list_facts_producer_tests qindaqt_task_list_operation_adapter_tests \
  qindaqt_task_list_operation_results_tests
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/negative-5b93-debug \
  -R '^qindaqt\.task-list-(wire|facts-producer|operation-adapter|operation-results)$' \
  --output-on-failure --no-tests=error
```

Results: source equivalence exit 0; archive/configure/build exit 0; CTest exit
8 as expected, 0/4 suites passed and 12 hostile assertions failed (3/4/1/4 by
suite). The extracted source and build stayed under the assigned build root.

### Scratch regression/reproduction probe

For `PROFILE=debug` and then `PROFILE=release`:

```sh
env TMPDIR=/home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/repro \
  c++ -std=c++23 -fPIC \
  -I/home/cabewse/work_SPaC3/container-wm-workers/task-list-t1-codex-review/src/shell/task_list/include \
  -I/home/cabewse/work_SPaC3/container-wm-workers/task-list-t1-codex-review/src/shell/task_list/producer/include \
  -I/home/cabewse/work_SPaC3/container-wm-workers/task-list-t1-codex-review/src/shell/task_list/operations/include \
  $(pkg-config --cflags Qt6Core Qt6DBus) \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/repro/r2_probe.cpp \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/$PROFILE/src/shell/task_list/operations/libqindaqt_shell_task_list_operations.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/$PROFILE/src/shell/task_list/producer/libqindaqt_shell_task_list_producer.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/$PROFILE/src/shell/task_list/libqindaqt_shell_task_list.a \
  $(pkg-config --libs Qt6DBus Qt6Core) \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/repro/r2-probe-$PROFILE
/home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/repro/r2-probe-$PROFILE
```

Both compile and run commands: exit 0. Both runs produced the same three lines
quoted in the findings/regression sections. Preliminary reviewer-only compile
attempts exposed a Qt `signals` macro name collision and then a missing `-fPIC`;
the scratch source/command were corrected before evidence collection. No product
path was edited.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/site
./tools/check-source-shape
git diff --check
git diff --check 3a5ae17..HEAD
git diff --check 50c2162..HEAD
while IFS= read -r path; do python3 -m json.tool "$path" >/dev/null || exit 1; done \
  < <(git diff --name-only 3a5ae17..HEAD -- '*.json')
```

- `validate-docs`: exit 0, 129 Markdown documents plus navigation validated.
- strict MkDocs: exit 0.
- source shape: exit 0, 2,118 files checked; four threshold warnings were in
  unrelated pre-existing/merged files.
- all three diff checks: exit 0.
- JSON validation: exit 0, all 17 JSON paths in the broad candidate/ancestor
  diff parsed.

## Verdict

The repair closes the prior 0/3/1/1 ledger, but the exact descendant still
accepts an impossible Dock success as committed and strands the initial
no-owner state in Loading. ACCEPT requires zero P0-P2 findings.

VERDICT REJECT P0/P1/P2/P3=0/1/1/0
