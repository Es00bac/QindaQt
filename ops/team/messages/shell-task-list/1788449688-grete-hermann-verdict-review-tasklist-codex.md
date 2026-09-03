# Grete Hermann — independent shell review

- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `3a5ae1773cac99c0a5e67b1785cf79a9a262c8b4`
- Tree SHA: `3a79129bd56759afffe43dca0f7cc6f54c024e8b`
- Parent SHA: `6a016c6f18edec395803a14d27ad13fa781432bb`
- Base SHA: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/task-list-t1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex`
- Review result: **REJECT**

## Review-question summary

1. **Facts are not proof-bound.** The transport does bind calls/signals to an exact unique owner, and owner loss after a successful generation retains the last generation as Degraded. However, the producer combines two inventories that the normative Compositor1 reference expressly says must not be combined, ignores `ShellVisibilitySnapshot` epoch/revision after decoding, and accepts incomplete/foreign-lineage scope payloads as current truth. Production composition is excluded, so absence of a second runtime compositor client is not qualified by this candidate.
2. **Operation serialization is bounded in the ordinary lifetime, but exact lineage fails.** One adapter instance has one in-flight request and a timeout, and duplicate/unknown `(token, owner)` signals are ignored. Tokens restart at 1 when the adapter is reconstructed while the transport may retain old pending calls, so a late reply from the previous adapter can terminate the new request. Submit replies are also accepted from status alone without the protocol/transaction/container/revision echo.
3. **Text and collection bounds are partly sound.** The decoder rejects over-limit payloads, dirty title/identifier text, and collections above 4,096; a scratch run decoded and joined 4,096 standalone windows in 35 ms Debug and 20 ms Release, and rejected 4,097. But incomplete/unknown-output visibility snapshots are accepted rather than rejected wholesale.
4. **Tests are isolated but incomplete.** The 13 registered rows pass in Debug and Release with inherited session-bus state removed. The private-bus row launches only its own `dbus-daemon`; no QML/offscreen rows exist. There is no registered foreign/stale epoch, post-Ready stop, degradation notification, semantic Submit reply, adapter-reconstruction lineage, or 4,096/4,097 T1 wire row.
5. **Scope/shape gates pass.** All changed paths are owned or allowed additive documentation/build paths; no JSON or `mkdocs.yml` changed. Presentation/applet/runtime composition stays excluded. The page nevertheless overclaims coherent production facts and exact reply behavior, and its future window-operation direction is stale relative to current `main` ADR-0061.

## Findings ledger

### P0

None.

### P1

#### P1-1 — The facts producer accepts torn, foreign-lineage scope truth

The normative reference says `ShellVisibilitySnapshot` is a separate panel-visibility inventory and “clients must not combine” it with `Windows()` (`docs/wiki/reference/compositor-control-v1.md:112-117`). It also requires complete-snapshot validation and `(owner, epoch, revision)` lineage with older epochs and regressions rejected (`:156-160`, `:172-192`). The candidate instead documents and performs that forbidden join (`docs/wiki/shell/task-list.md:88-97`; `src/shell/task_list/producer/src/task_list_facts_producer.cpp:273-298`). Its decoder reads only status/schema/epoch/revision/windows, ignores required `outputGeneration`, `scope`, and `outputs`, does not validate the epoch as a UUID or output membership (`src/shell/task_list/producer/src/task_list_wire.cpp:329-392`), and the producer retains no accepted scope lineage (`task_list_wire.h:73-79`). A signal-race bit cannot prove two independently sampled methods are one generation.

Reproduction (scratch source is outside the worktree at `<ROOT>/repro/repro.cpp`): compile/run the command recorded below. Both profiles print:

```text
foreign_epoch status=1 revision=2 output=output-foreign stateChanged=1
```

The first accepted snapshot has epoch `aaaaaaaa-...`, revision 9 and `output-current`. Under the same owner, the reproduction then supplies a scope payload with foreign epoch `bbbbbbbb-...`, regressed revision 1, `output-foreign`, and none of the required top-level `outputGeneration`, `scope`, or `outputs` fields. Observed: `status=1` (`Ready`), source revision advances to 2, and foreign output truth is published. Expected: reject the complete sample and withdraw/degrade until a lineage-coherent task-list snapshot exists. Because `Windows()` has no compatible generation, the durable fix requires a coherent compositor task-list inventory/boundary rather than merely comparing the visibility revision.

#### P1-2 — Failure and stop lifecycle do not publish fail-closed availability

`TaskListFactsProducer::stateChanged` promises notification when source status changes (`task_list_facts_producer.h:60-63`). `failRefresh()` changes the source to Degraded but never calls `emitStateIfChanged()` (`task_list_facts_producer.cpp:301-321`); the join-failure path has the same omission (`:290-296`). `stop()` clears timers/transport only, leaving the accepted source, owner, and container lineage live (`:90-119`). The operation adapter admits solely from those retained accessors (`task_list_operation_adapter.cpp:424-472`).

Reproduction: the same scratch command prints in both profiles:

```text
degrade_signal status=2 stateChanged=0
after_stop status=1 owner=:1.3 operation_calls=1
```

Observed: a malformed refresh changes the underlying source to `Degraded` (`2`) without emitting the only producer notification, and stopping a previously Ready producer leaves status `Ready` (`1`) plus the old owner; a subsequent `releaseContainer()` reaches the operation transport once. Expected: every observable degradation is signaled, and `stop()` withdraws/degrades owner-bound truth so no mutation can be admitted.

#### P1-3 — Operation reply lineage can be forged or recycled across adapter lifetimes

For Submit, the compositor's canonical reply includes `protocol`, `transactionId`, `containerId`, `status`, and `revision` (`src/compositor/src/controlcodec.cpp:145-163`). The adapter treats a matching status alone as success and never validates that reply lineage (`src/shell/task_list/operations/src/task_list_operation_adapter.cpp:317-373`). Its local token counter also starts at 1 in every adapter instance (`task_list_operation_adapter.h:104-105`; `task_list_operation_adapter.cpp:492-498`), while `QtTaskListOperationTransport` owns pending watchers independently. Reconstructing the adapter over the same transport can therefore recycle `(token, owner)` and let an old reply terminate a new request.

Reproduction: the same scratch command prints:

```text
semantic_malformed_reply status=0
adapter_recreation oldToken=1 newToken=1 status=11 newInFlight=0
```

Observed: a Submit reply containing only `{"status":"committed"}` becomes `Committed` (`0`) despite missing every required echo. Separately, adapter A sends Release with token 1 and is destroyed; adapter B sends Dock with recycled token 1, then adapter A's late `released` reply is delivered. Adapter B incorrectly terminates as `Uncertain` (`11`) with `newInFlight=0`. Expected: semantically incomplete/mismatched Submit replies are Uncertain, and a late reply from a previous adapter instance is unknown lineage and leaves adapter B's request in flight.

### P2

#### P2-1 — Required hostile/stress negative controls are not registered

`tests/shell/task_list/tst_task_list_wire.cpp:25-34` and `tst_task_list_facts_producer.cpp:64-73` contain no at-limit/over-limit T1 Windows inventory, same-owner epoch replacement/regression, post-Ready stop, or degradation-signal case. The operation rows likewise have no semantic Submit echo validation or adapter reconstruction case; their nominal Submit success fixture is itself incomplete (`tst_task_list_operation_results.cpp:172-205`).

Reproduction:

```sh
rg -n "4096|4'096|4097|4'097|MaxWindows|foreign.*epoch|epoch.*regress|stop.*Ready|Ready.*stop|stateChanged.*Degrad|Degrad.*stateChanged" tests/shell/task_list
```

Exit 1, zero matches. Observed: removing the T1 `Windows()` count guard or retaining the P1 lifecycle/epoch defects does not fail a registered row. Expected: registered hostile rows at 4,096/4,097 and deliberate broken-variant controls for each claimed lineage/lifecycle behavior. The external scratch check confirms the current count implementation happens to handle the bound, but it is not durable registered evidence.

### P3

#### P3-1 — The window-operation extension section is stale against current `main`

Against the candidate's own base, returning Unavailable for ordinary window actions is honest. Current `main` has accepted ADR-0061 and `src/shell_window_actions_client`, providing authenticated `CompositorShell1` activate/minimize/unminimize/close/raise. Therefore `docs/wiki/shell/task-list.md:144-153` must not drive a new Compositor1 extension, and later task-list composition must consume the existing public action client rather than duplicate or bypass it. ADR-0063's authenticated active-window identity remains a separate single-client concern; this candidate should not grow another identity reader.

## Commands and results

### Identity, cleanliness, and scope

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse f350028
git status --porcelain=v1
git diff --name-status f350028..3a5ae1773cac99c0a5e67b1785cf79a9a262c8b4
git diff --stat f350028..3a5ae1773cac99c0a5e67b1785cf79a9a262c8b4
```

Exit 0. The SHAs match the header; status output was empty before review. The complete diff contains 31 paths, all under the owned task-list source/tests or the allowed additive wiki pages. No JSON changed.

Required reading completed: `AGENTS.md`, wiki index, module boundaries, coding practices, task-list page, ADR-0044, Compositor1 reference, hybrid topology, window-container model, lane brief, and timestamped implementer handoff. Current-main ADR-0061/ADR-0063 were read with `git show main:...`. The named resume artifact `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/task-list-t1-resume/last-message.md` does not exist (`sed` exit 2); the resume `brief.md` and `run.log` do exist and were inspected.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/dev -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both exit 0. CMake emitted existing mixed-prefix runtime-path warnings; generation completed.

### Focused build

For each of `dev` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/<profile> --parallel 3 --target qindaqt_shell_task_list qindaqt_shell_task_list_producer qindaqt_shell_task_list_operations qindaqt_task_list_values_tests qindaqt_task_list_source_grouping_tests qindaqt_task_list_source_validation_tests qindaqt_task_list_intents_tests qindaqt_task_list_scope_filter_tests qindaqt_task_list_presentation_tests qindaqt_task_list_wire_tests qindaqt_task_list_fact_joiner_tests qindaqt_task_list_facts_producer_tests qindaqt_task_list_operation_adapter_tests qindaqt_task_list_operation_results_tests qindaqt_task_list_qt_transports_tests
```

Both exit 0; strict-warning focused targets build cleanly.

### Focused tests

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/dev -R '^qindaqt\.task-list-' --output-on-failure --no-tests=error
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/release -R '^qindaqt\.task-list-' --output-on-failure --no-tests=error
```

Debug exit 0, 13/13 passed. Release exit 0, 13/13 passed. No `tests/session`, host D-Bus, display, hardware, uinput, or network row was run. There are no task-list QML/offscreen rows.

### Scratch reproductions

Debug compile/run:

```sh
c++ -std=c++20 -fPIC /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/repro/repro.cpp -Isrc/shell/task_list/include -Isrc/shell/task_list/producer/include -Isrc/shell/task_list/operations/include -Isrc/shell_visibility_protocol/include $(pkg-config --cflags Qt6Core Qt6DBus) /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/dev/src/shell/task_list/operations/libqindaqt_shell_task_list_operations.a /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/dev/src/shell/task_list/producer/libqindaqt_shell_task_list_producer.a /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/dev/src/shell/task_list/libqindaqt_shell_task_list.a $(pkg-config --libs Qt6DBus Qt6Core) -o /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/repro/repro-dev && env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/repro/repro-dev
```

Release compile/run is identical with `-DNDEBUG`, every `/dev/` library path changed to `/release/`, and output `repro-release`. Both exit 0 and reproduce the same four failures plus the adapter-recreation failure. Debug reports `window_bound decoded=4096 joined=4096 elapsed_ms=35 over_limit_error=12`; Release reports 20 ms. Error 12 is `TaskListWireError::LimitExceeded`.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-codex/site
./tools/check-source-shape
git diff --check
git diff --check f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9..3a5ae1773cac99c0a5e67b1785cf79a9a262c8b4
```

All exit 0. `validate-docs`: 117 Markdown documents/navigation validated. MkDocs strict build completed. Source shape checked 1,814 files and emitted only two pre-existing warnings outside task-list paths (`tests/compositor/CMakeLists.txt` 500 and `tests/services/display_color_model/tst_color_model.cpp` 539 nonblank lines). Both diff checks are clean. JSON gate is not applicable because no JSON changed.

Final post-review commands:

```sh
git rev-parse HEAD
git status --porcelain=v1
```

Result: `3a5ae1773cac99c0a5e67b1785cf79a9a262c8b4`; status output empty.

## Verdict

The candidate has no host-affecting behavior in the reviewed tests, but its central proof-bound facts and exactly-once reply-lineage claims fail executable reproductions. It is not eligible for integration until all P1/P2 findings are repaired and the exact repaired commit is re-reviewed.

VERDICT REJECT P0/P1/P2/P3=0/3/1/1
