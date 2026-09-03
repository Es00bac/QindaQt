# Vivienne Malone-Mayes-Codex — task-list T1 repair 2 handoff

- Candidate commit: `bf555ed73ecc761e32389109dcbbf9f529c6570e`
- Candidate tree: `be1413f49e29d9774c0a4c43727a9cfb7744c5f7`
- Candidate parent: `2887e3823c5f7a1172a34d540ca95ff5e7d5929c`
- Rejected candidate: `7b6bd8ac74511a0cbbe6fb4088305655ad047340`
- Exact lane/product base: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`
- Branch: `worker/task-list-t1`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/task-list-t1`
- Requested next action: **independent exact review by Grete Hermann, then manager integration**.

## Finding closure

- **P1-1 — impossible `DockWindows` revision classified as committed:** closed by `bf555ed73ecc761e32389109dcbbf9f529c6570e`. A `docked` reply now commits only at the Compositor1-fixed revision `"1"`; canonical revisions `"0"` and `"2"` settle as `Uncertain` with `reply-lineage-mismatch`. Registered control: `qindaqt.task-list-operation-lineage::dockSuccessRevisionMustBeExactlyOne` (`AGENT-NOTE` names P1-1 and rejected candidate `7b6bd8a`).
- **P2-1 — cold start without a compositor owner silently remains Loading:** closed by `bf555ed73ecc761e32389109dcbbf9f529c6570e`. The transport now distinguishes unresolved owner discovery from its first published owner observation. A resolved unowned service publishes one empty observation, producing `Degraded`, a stable unavailable reason, and one `stateChanged`. Registered control: `qindaqt.task-list-qt-transports::initialUnownedCompositorDegradesProducer` on a fresh private bus (`AGENT-NOTE` names P2-1 and rejected candidate `7b6bd8a`).

## Changed paths in the candidate (sorted)

```text
docs/wiki/development/testing-harness.md
docs/wiki/shell/task-list.md
src/shell/task_list/operations/include/qindaqt/shell/task_list/operations/task_list_operation_reply.h
src/shell/task_list/operations/src/task_list_operation_reply.cpp
src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/qt_task_list_producer_transport.h
src/shell/task_list/producer/src/qt_task_list_producer_transport.cpp
src/shell/task_list/producer/src/task_list_facts_producer.cpp
tests/shell/task_list/tst_task_list_operation_lineage.cpp
tests/shell/task_list/tst_task_list_qt_transports.cpp
```

## Reproduction and negative controls

Grete's exact `r2_probe.cpp` was compiled against the rejected product libraries in each assigned Debug and Release profile with `c++ -std=c++23 -fPIC`, the three task-list include roots, `pkg-config --cflags/--libs Qt6Core Qt6DBus`, and the profile's three task-list static libraries; `TMPDIR` and both output binaries were under `/home/cabewse/work_SPaC3/builds/qindaqt/task-list-t1/repair2-repro`. Both compile/run pairs exited 0. Before repair both printed:

```text
dock_zero_revision verdict=0
initial_unowned started=1 status=0 owner_empty=1 signals=0 error=
```

Thus P1-1 reproduced as `Committed`, and P2-1 reproduced as `Loading` with no notification or reason.

After adding only the registered controls and rebuilding the two test executables against the rejected product libraries, this ran:

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/task-list-t1/debug -R '^qindaqt\.task-list-(operation-lineage|qt-transports)$' --output-on-failure --no-tests=error
```

Result: exit 8 as expected, 0/2 suites passed. The Dock control observed status 0 instead of expected `Uncertain`; the private-bus control observed status 0 instead of expected `Degraded`. A combined Debug/Release attempt also showed the Release Dock control failing; its output collection ended during the longer Release no-owner failure, so no complete Release negative-control count is claimed. The two-profile reviewer probe above independently reproduced both findings in Release.

After repair, the same two-row selector passed with exit 0 and 2/2 suites in Debug and Release. The final probe compile/run pairs also exited 0 and both printed:

```text
dock_zero_revision verdict=4
initial_unowned started=1 status=2 owner_empty=1 signals=1 error=compositor owner is unavailable
```

Enum value 4 is `UncertainLineage`; source status 2 is `Degraded`.

## Required verification

Both prescribed configure commands ran with the assigned profile directory, private KWin 6.6.5 initial cache, `BUILD_TESTING=ON`, all three shell/plugin build flags, host uinput off, and strict warnings on:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/task-list-t1/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Release used the same command with `release` and `-DCMAKE_BUILD_TYPE=Release`. Both exited 0; existing mixed-prefix RPATH warnings were emitted.

The full focused target set was built in each profile:

```sh
env TMPDIR=<ROOT>/<profile>/tmp cmake --build <ROOT>/<profile> --parallel 3 --target qindaqt_shell_task_list qindaqt_shell_task_list_producer qindaqt_shell_task_list_operations qindaqt_task_list_values_tests qindaqt_task_list_source_grouping_tests qindaqt_task_list_source_validation_tests qindaqt_task_list_intents_tests qindaqt_task_list_scope_filter_tests qindaqt_task_list_presentation_tests qindaqt_task_list_wire_tests qindaqt_task_list_facts_producer_tests qindaqt_task_list_operation_adapter_tests qindaqt_task_list_operation_results_tests qindaqt_task_list_operation_lineage_tests qindaqt_task_list_qt_transports_tests
```

Debug and Release builds: exit 0, 15 named targets each.

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/debug -R '^qindaqt\.task-list-' --output-on-failure --no-tests=error
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/release -R '^qindaqt\.task-list-' --output-on-failure --no-tests=error
```

- Debug: exit 0, 13/13 passed, 0 failed.
- Release: exit 0, 13/13 passed, 0 failed.
- The selector contains no QML rows, so `QT_FATAL_WARNINGS=1` was not applicable.

Static gates:

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/task-list-t1/site
./tools/check-source-shape
git diff --check
git diff --check 7b6bd8ac74511a0cbbe6fb4088305655ad047340..bf555ed73ecc761e32389109dcbbf9f529c6570e
```

- `validate-docs`: exit 0; 129 Markdown documents and navigation validated.
- strict MkDocs: exit 0.
- source shape: exit 0; 2,118 files checked. Four decomposition warnings are pre-existing and outside owned paths (`tests/apps/settings_center/tst_settings_navigation_page.cpp`, `tests/compositor/CMakeLists.txt`, `tests/services/display_color_model/tst_color_model.cpp`, and `tests/shell/audio_applet/tst_audio_applet_controller.cpp`). `tests/session/DesktopSessionTests.cmake` is 499 non-blank lines here and did not warn.
- both diff checks: exit 0.
- No JSON changed, so no JSON syntax command was applicable.

No `tests/session`, nested compositor, host D-Bus, hardware, uinput, or network row was run.

## Bounded caveats

- This repair changes only operation reply classification and producer owner-discovery availability. It does not claim a coherent public task-list inventory, production-shell instantiation, QML rendering, or live desktop task-list behavior.
- Compositor1 1.1 still cannot publish the full T0 task-list generation; the producer remains deliberately fail-closed until that separate compositor prerequisite exists.
- No nested-session, physical hardware, host bus, uinput, or network evidence is claimed.

Requested next action: **independent exact review by Grete Hermann, then manager integration**.
