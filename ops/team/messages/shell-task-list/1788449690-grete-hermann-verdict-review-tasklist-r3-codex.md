# Exact-candidate repair recheck — Grete Hermann

- Persona: Grete Hermann
- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Candidate SHA: `bf555ed73ecc761e32389109dcbbf9f529c6570e`
- Tree SHA: `be1413f49e29d9774c0a4c43727a9cfb7744c5f7`
- Parent SHA: `2887e3823c5f7a1172a34d540ca95ff5e7d5929c`
- Product base SHA: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`
- Product base tree SHA: `4f2ceebbe8c28145c6e6e6cfd602d67bbaf33d11`
- Rejected ancestor: `7b6bd8ac74511a0cbbe6fb4088305655ad047340`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/task-list-t1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-r2-codex`

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Recheck of the rejected findings

### Prior P1-1 — closed

The protocol fixes a successful `DockWindows` result at revision `"1"`
(`docs/wiki/reference/compositor-control-v1.md:46,55-61`). The repaired
classifier now requires that exact value before returning `Committed`
(`src/shell/task_list/operations/src/task_list_operation_reply.cpp:142-163`).
The registered control supplies canonical but impossible revisions `"0"` and
`"2"`, and requires `Uncertain` plus `reply-lineage-mismatch`
(`tests/shell/task_list/tst_task_list_operation_lineage.cpp:176-198`).

The control is mutation-sensitive. I archived exact `7b6bd8a`, overlaid only
the two registered repair test sources from `bf555ed`, and built against the
archived rejected product code. In both Debug and Release,
`dockSuccessRevisionMustBeExactlyOne` failed at line 194: actual status `0`
(`Committed`), expected status `11` (`Uncertain`). Against `bf555ed`, the full
registered selector passes in both profiles and the unchanged prior probe
reports `dock_zero_revision verdict=4` (`UncertainLineage`).

### Prior P2-1 — closed

The transport now records whether the initial owner observation has been
published. An unresolved empty value remains Loading, while the first resolved
empty observation emits `serviceOwnerChanged({})`; stop resets the observation
state for a later restart
(`src/shell/task_list/producer/src/qt_task_list_producer_transport.cpp:68-91,133-203`).
The producer maps that observation to a stable unavailable error and Degraded
state (`src/shell/task_list/producer/src/task_list_facts_producer.cpp:133-157`),
matching the documented three-state discovery contract
(`docs/wiki/shell/task-list.md:90-93`).

The private-bus registered control requires Degraded, an empty owner, an
`unavailable` error, and exactly one `stateChanged`
(`tests/shell/task_list/tst_task_list_qt_transports.cpp:142-169`). On the exact
archived `7b6bd8a` product, that test failed in both Debug and Release at line
162: actual status `0` (`Loading`), expected status `2` (`Degraded`). Against
`bf555ed`, the full selector passes in both profiles and the unchanged prior
probe reports:

```text
initial_unowned started=1 status=2 owner_empty=1 signals=1 error=compositor owner is unavailable
```

### Earlier closures and regression view

The complete 13-row task-list selector passed in both profiles. This retains
the earlier registered controls for coherent-inventory refusal, owner/epoch/
revision fencing, invalidation races, owner replacement, stop behavior,
operation admission, exact Submit lineage, adapter-lifetime token fencing,
wire bounds, and the source boundary. The unchanged prior probe also retained
the exact hostile text boundary result in both profiles:

```text
wire_bounds title512=0 title513=6 app512=0 app513=6
```

The repair diff updates only the task-list transport/reply behavior, its two
registered tests, and matching task-list/testing documentation; the intervening
team-message and worker-record additions are additive. No production-shell
composition, coherent task-list inventory, QML surface, nested compositor, or
hardware behavior is claimed.

## Commands and results

### Identity, ancestry, diff, and cleanliness

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git status --porcelain
git show -s --format='candidate=%H%ntree=%T%nparents=%P' HEAD
git show -s --format='base=%H%ntree=%T%nparents=%P' f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9
git merge-base --is-ancestor f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9 HEAD
git merge-base --is-ancestor 7b6bd8ac74511a0cbbe6fb4088305655ad047340 HEAD
git diff --stat 7b6bd8ac74511a0cbbe6fb4088305655ad047340..bf555ed73ecc761e32389109dcbbf9f529c6570e
git diff --name-status 7b6bd8ac74511a0cbbe6fb4088305655ad047340..bf555ed73ecc761e32389109dcbbf9f529c6570e
git diff 7b6bd8ac74511a0cbbe6fb4088305655ad047340..bf555ed73ecc761e32389109dcbbf9f529c6570e -- \
  docs/wiki/development/testing-harness.md docs/wiki/shell/task-list.md \
  src/shell/task_list tests/shell/task_list
```

Results: identity values match the header; both ancestry checks exited 0.
`git status --porcelain` was empty before and after review. The repair product
commit changes nine paths (113 insertions, 11 deletions); the broader descendant
diff also contains four additive team coordination files from the intervening
handoff commit.

### Required configure and focused build

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-r2-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-r2-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both configure commands exited 0. They emitted the existing mixed-prefix RPATH
warnings.

For each exact profile directory, I ran:

```sh
cmake --build <exact-debug-or-release-directory> --parallel 3 --target \
  qindaqt_shell_task_list qindaqt_shell_task_list_producer \
  qindaqt_shell_task_list_operations qindaqt_task_list_values_tests \
  qindaqt_task_list_source_grouping_tests qindaqt_task_list_source_validation_tests \
  qindaqt_task_list_intents_tests qindaqt_task_list_scope_filter_tests \
  qindaqt_task_list_presentation_tests qindaqt_task_list_wire_tests \
  qindaqt_task_list_facts_producer_tests qindaqt_task_list_operation_adapter_tests \
  qindaqt_task_list_operation_results_tests qindaqt_task_list_operation_lineage_tests \
  qindaqt_task_list_qt_transports_tests
```

Both builds exited 0 for all 15 requested targets. Each rebuilt 74 Ninja edges;
the final Debug confirmation reported `ninja: no work to do`.

### Candidate tests

`ctest -N` enumerated exactly 13 matching rows and no QML row. Therefore
`QT_FATAL_WARNINGS=1` was not applicable.

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-r2-codex/debug \
  -R '^qindaqt\.task-list-' --output-on-failure --no-tests=error

env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-r2-codex/release \
  -R '^qindaqt\.task-list-' --output-on-failure --no-tests=error
```

- Debug: exit 0, 13/13 passed, 0 failed, 1.65 s.
- Release: exit 0, 13/13 passed, 0 failed, 1.58 s.
- Correctly ordered before/after PID comparisons found no daemon left by either
  candidate run.

No `tests/session`, nested compositor, host bus, hardware, uinput, or network
row was run.

### Registered negative controls on exact `7b6bd8a`

I created the scratch tree at
`/home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-r2-codex/negative-7b6bd8a-Wm0dN2`:

```sh
git archive --format=tar 7b6bd8ac74511a0cbbe6fb4088305655ad047340 | tar -x -C <scratch>
git archive --format=tar bf555ed73ecc761e32389109dcbbf9f529c6570e \
  tests/shell/task_list/tst_task_list_operation_lineage.cpp \
  tests/shell/task_list/tst_task_list_qt_transports.cpp | tar -x -C <scratch>
sha256sum tests/shell/task_list/tst_task_list_operation_lineage.cpp \
  <scratch>/tests/shell/task_list/tst_task_list_operation_lineage.cpp
sha256sum tests/shell/task_list/tst_task_list_qt_transports.cpp \
  <scratch>/tests/shell/task_list/tst_task_list_qt_transports.cpp
```

Both test-source hash pairs matched. `git diff --no-index --quiet` also verified
that the two relevant product implementation files in the scratch tree were
byte-identical to `7b6bd8a`.

For Debug and Release I configured the standalone task-list suite with the same
initial cache, `BUILD_TESTING=ON`, strict warnings, and the corresponding
`CMAKE_BUILD_TYPE`, then ran:

```sh
cmake --build <scratch>/<profile> --parallel 3 --target \
  qindaqt_task_list_operation_lineage_tests qindaqt_task_list_qt_transports_tests
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <scratch>/<profile> \
  -R '^qindaqt\.task-list-(operation-lineage|qt-transports)$' \
  --output-on-failure --no-tests=error
```

Both configurations and builds exited 0. Both CTest runs exited 8 as expected,
0/2 suites passed and exactly the two intended assertions failed: Dock status
was `Committed` rather than `Uncertain`, and no-owner status was `Loading`
rather than `Degraded`. The Debug and Release private-bus test destructors left
no daemon matching their exact command line.

During the Release negative-control cleanup audit, a reviewer script used
numeric rather than lexicographic sorting before `comm`. It consequently
misclassified and sent SIGTERM to 16 pre-existing private test daemons whose
command line was exactly `--session --nofork --nopidfile --print-address=1`.
It did not target the system bus, user session bus, or accessibility buses. The
comparison was corrected before subsequent probes; this harness incident does
not alter candidate files or the independently completed candidate results.

### Unchanged prior reproduction probe

The prior source hash was retained exactly as
`403b0efd71e0bf70521376d716d9e50a2492dd864dee165dc1ba5a000310d937`.
I compiled it separately against the Debug and Release candidate static
libraries with the same C++23/Qt6 flags recorded in the prior verdict, then ran
each binary under the required cleared session-bus environment. Both compile
and run pairs exited 0 and printed:

```text
wire_bounds title512=0 title513=6 app512=0 app513=6
dock_zero_revision verdict=4
initial_unowned started=1 status=2 owner_empty=1 signals=1 error=compositor owner is unavailable
```

Correctly ordered PID comparisons found no daemon left by either probe.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tasklist-r2-codex/site-r3
./tools/check-source-shape
git diff --check
git diff --check 7b6bd8ac74511a0cbbe6fb4088305655ad047340..bf555ed73ecc761e32389109dcbbf9f529c6570e
git diff --name-only 7b6bd8ac74511a0cbbe6fb4088305655ad047340..bf555ed73ecc761e32389109dcbbf9f529c6570e -- '*.json'
```

- `validate-docs`: exit 0, 129 Markdown documents plus navigation validated.
- strict MkDocs: exit 0.
- source shape: exit 0, 2,118 files checked; four warnings are unrelated
  pre-existing threshold files and no repair path is among them.
- both diff checks: exit 0.
- changed-JSON query: exit 0 with no paths, so `python3 -m json.tool` was not
  applicable.

## Verdict

Both rejected findings are repaired, each registered control demonstrably
fails at its intended assertion on exact `7b6bd8a`, all earlier closures remain
green, and the exact descendant passes the required Debug, Release, and static
gates. There are no candidate P0-P2 findings.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
