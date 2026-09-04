# Exact-candidate review verdict — Task List T3 production shell hosting

- Persona: Henrietta Leavitt, independent shell reviewer (`henrietta-leavitt`)
- Provider/model: Moonshot Kimi `kimi-code/k3`
- Candidate SHA: `e78e407dd452426b03f60cb3155329574f43c36a` (`worker/task-list-hosting`, implementer Vera Rubin, OpenAI Codex)
- Tree SHA: `fedd9c98b840ac44d84c37d378765f9b6fb8c20a` (matches implementer handoff)
- Parent SHA: `e742d63265b9814ed55d2a5f9f1aee88647ce4bb`
- Base SHA: `e742d63265b9814ed55d2a5f9f1aee88647ce4bb`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/task-list-hosting-k3-review`
- Build root (`<ROOT>`): `/home/cabewse/work_SPaC3/builds/qindaqt/review-tlhost-k3`
- `git rev-parse HEAD` equals the candidate SHA and `git status --porcelain` was empty before and after all work. No product path was edited; all output stayed under `<ROOT>`.

## Findings ledger

### P0 — none

### P1 — none

Composition audit (review question 1): `src/shell/runtime/tasklistappletcomposition.cpp`
borrows the shell-owned exact-owner `ShellWindowActionsClient` by reference; the only
transports constructed are the T1 Compositor1 producer/operation transports on the
shared session bus — no second `CompositorShell1` binding, no authority in QML (QML
receives only the shell-private `TaskListAppletController`). The router fences every
dispatch on `outcome.ok()`, authority `Ready`, `expectedRevision == publishedRevision`,
non-empty matching unique owners, a valid action generation, and client availability;
unbound/mismatched owners finish `Unavailable` before bus traffic. Operations serialize
through one `m_pending`; Close All / container minimize-unminimize send one request per
canonical member and emit exactly one terminal result; uncertain/timeout/owner-change
results are never replayed. Owner loss clears retained truth twice (producer
`handleServiceOwnerChanged` source reset in `task_list_facts_producer.cpp:144-157`, plus
the router's authority-owner watch with the AGENT-GUARD at
`tasklistappletcomposition.cpp:130-136`). The private-bus row proves all five actions
round-trip, grouped sequencing order, the generation echo `(identity-epoch, 7)`, exactly-once
terminal outcome with a late raced reply, and stale-truth clearing to `degraded`/empty.

Hosted QML (question 2): the new `qindaqt.task-list-applet-panel-dispatcher` qmltestrunner
row runs under `QT_FATAL_WARNINGS=1` with `DBUS_SESSION_BUS_ADDRESS`/`DISPLAY`/
`WAYLAND_DISPLAY` unset and proves horizontal row, vertical column, and disabled preview
hosting through the test-import stub that mirrors the production `access`/`vertical`
boundary. Keyboard traversal, accessible names/roles/states, and bounded overflow are
covered by the re-run fatal-warning offscreen rows (`-qml-offscreen`, `-qml-arrows-offscreen`,
`-qml-keyboard-offscreen`). None of these is source-only or tautological: assertions
compare exact dispatched kinds, task ids, revisions, and visible/enabled state.

Packaging (question 3): `qindaqt_install_task_list_applet_runtime` is applied to all eight
shell-carrying components; `qindaqt.shell-runtime-component-closure` now rejects a missing
`QindaQt/Shell/TaskList` qmldir/QML payload per isolated component stage and passes. The
relocated installed-package row reruns after a stage move with `LD_LIBRARY_PATH` unset and
adds a source-poison probe (`--list` under hostile ambient paths). The dispatcher count is
pinned by `check_launcher_contract_text.cmake` ("all nine") and the registry/dispatcher
inventory matches: clock, notification-center, audio, bluetooth, power, clipboard, launcher,
global-menu, task-list. DesktopVirtual staging of the TaskList module pre-existed in
`tests/session/PanelVisibilityTests.cmake` and `desktop.virtual.stage-closure` passes with
its missing-library and missing-qmldir negative controls. The five profile edits are purely
additive one-applet insertions, validated by `stockProfilesPlaceHostedTaskListWhereWorkflowExposesTasks`.

### P2 — none

### P3

1. Stale AGENT-NOTE, `tests/session/PanelVisibilityTests.cmake:107-110`: "The production
   shell does not link the task-list applet library yet (dispatcher wiring/hosting is a
   later lane)." This candidate wires exactly that (`src/shell/CMakeLists.txt` links
   `QindaQt::ShellTaskListApplet` into `qindaqt-shell`; `BuiltinAppletContent.qml` hosts
   it). AGENTS.md declares a stale marker a defect. Nonblocking: the staging rule itself
   is correct and now load-bearing. Reproduction: read the comment against
   `src/shell/CMakeLists.txt:229` and `src/shell/qml/BuiltinAppletContent.qml:221-228`.
   Related precision: `tests/session/DesktopVirtualAppletModules.cmake:3` claims its
   five-entry list is the "complete module set" imported by `BuiltinAppletContent.qml`,
   while Clipboard and TaskList are staged from `PanelVisibilityTests.cmake` instead; the
   aggregate closure is proven by the passing stage-closure row, so this is comment
   precision only.

2. Imprecise terminal status, `src/shell/runtime/tasklistappletcomposition.cpp:155-161`:
   a not-`Ready` facts authority (Loading/Degraded race) finishes as `StaleGeneration`
   ("the displayed task generation is no longer current") although the enum offers
   `SourceNotReady`, which the T1 adapter uses for the same condition. Reachable only in a
   narrow race — the controller refuses degraded sources before dispatch — and the outcome
   remains a truthful pre-dispatch refusal, so this is precision, not a contract violation.

## Commands and results

All ctest invocations used `--output-on-failure --no-tests=error`; `<ROOT>` as above.

| Gate | Command | Result |
| --- | --- | --- |
| Debug configure | `cmake -S . -B <ROOT>/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON` | exit 0 (Configuring/Generating done) |
| Release configure | same recipe, `-B <ROOT>/release -DCMAKE_BUILD_TYPE=Release` | exit 0 |
| Debug build | `cmake --build <ROOT>/debug --parallel 3` | exit 0, 414 edges |
| Release build | `cmake --build <ROOT>/release --parallel 3` | exit 0 |
| Debug mandated selector | `ctest --test-dir <ROOT>/debug -R '^qindaqt\.(task-list-|applet|shell-runtime-|launcher-panel-dispatcher|notification-center-applet-offscreen)'` | 34/34 passed, 0 failed |
| Debug DesktopVirtual safe rows | `ctest --test-dir <ROOT>/debug -R '^desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)$'` | 3/3 passed |
| Release mandated selector | same selector, `<ROOT>/release` | 34/34 passed, 0 failed |
| Release DesktopVirtual safe rows | same selector, `<ROOT>/release` | 3/3 passed |
| Docs validation | `./tools/validate-docs` | exit 0, 141 Markdown documents + mkdocs.yml nav |
| Strict MkDocs | `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` | exit 0 |
| Source shape | `./tools/check-source-shape` | exit 0 |
| Whitespace | `git diff --check e742d632..HEAD` | clean |
| Changed JSON | `python3 -m json.tool` on the five changed `data/profiles/*.json` | all valid |

Not run (per lane rules): `tests/session` nested-compositor rows (`desktop.virtual.boot.1080p`,
`desktop.virtual.panel-visibility.*`), host D-Bus services, hardware, uinput, network. The
implementer's BLOCKED record covered only the nested-row KWin preflight, which the manager
runs separately.

## Verdict

ACCEPT requires P0=P1=P2=0; that bar is met. The two P3 items are comment/status precision
repairs for a follow-up, not integration blockers.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/2
