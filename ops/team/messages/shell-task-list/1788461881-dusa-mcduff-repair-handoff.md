# Dusa McDuff — Task list T2 repair handoff (bounded repair of da9f2fd)

- Candidate: `1e32f0a211cd6078652ca0d8d92750ec942b92c6`
- Tree: `4f391e41d09de7816c9e9637d56c0654d9cc1c3d`
- Exact base of the repair series: `da9f2fdcc54cd8b0ac2db971736f9ab466f72ad7` (the rejected candidate; its own base is `8cd28a796f84d866a867093567efbed3c0f01a2f`)
- Branch/worktree: `worker/task-list-applet` at `/home/cabewse/work_SPaC3/container-wm-workers/task-list-applet`
- Verdict repaired: Lauren Williams (OpenAI Codex) REJECT on `da9f2fd`, 0/2/1/1, at `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/review-tlapplet-codex/verdict.md`

## What changed per finding

- **P1.1 (dock pending fence)** — `task_list_applet_controller_intents.cpp` recorded the dock dispatch only against the target task. `PendingOperation` now carries a task-id list; the single dock token marks BOTH participants pending and the exactly-one terminal result releases both (header AGENT-GUARD records the invariant). New full-stack hostile row `dockDispatchFencesBothParticipantsUntilTerminalResult` in `tst_task_list_applet_operations.cpp` drives the real controller + bridge + adapter over the recording transport and asserts the incoming participant refuses activate/minimize/reverse-dock mid-flight with no second transport call. **Fails on da9f2fd**: same file copied into a `da9f2fd` worktree, standalone configure/build, `qindaqt_task_list_applet_operations_tests dockDispatchFencesBothParticipantsUntilTerminalResult` → qtest exit 1 (`FAIL! ... Compared values are not the same`, totals 2 passed/1 failed); passes on the candidate.
- **P1.2 (QST/Controls boundary)** — both `QindaQt.Shell.TaskList` QML files now `import QindaQt.Controls 1.0 as C` and `import QindaQt.Tokens 1.0`; the untyped `theme` property and all 18 hex fallback literals are gone (`rg '#[0-9A-Fa-f]{6}' src/shell/task_list/applet/qml` exits 1). Labels/dismiss button/focus ring are Controls primitives; remaining metrics resolve from Tokens semantic roles; the context menu keeps the QQC2 style palette (Controls ships no menu primitive; recorded in the wiki page). `check_task_list_applet_boundary.cmake` gained a probe-mode import/palette contract plus poison case 18; run against the da9f2fd tree it rejects with "task-list applet QML carries a hard-coded palette literal". Build order uses the launcher DEFER pattern (`qindaqt_task_list_applet_qml_dependencies`); the standalone `tests/shell/task_list` configure now adds themes/design_tokens/controls before the task-list module. The offscreen harness and the relocated installed-package consumer publish a real theme through the staged Tokens/Controls plugin path; the package harness stages both modules, rewrites RUNPATHs to `$ORIGIN`-relative, and asserts no absolute leaks on every staged ELF artifact (clipboard precedent). The dispatcher double (`tests/shell/qml/imports/.../TaskListApplet.qml`) drops `theme` to mirror the production boundary.
- **P2.1 (arrow traversal proof)** — new registered row `qindaqt.task-list-applet-qml-arrows-offscreen` (slot `arrowTraversalStopsAtStripEndpoints`): horizontal Left/Right and vertical Up/Down traverse in canonical order, stop at both endpoints, and the cross axis is inert, offscreen under `QT_FATAL_WARNINGS=1`; asserts zero dispatch.
- **P3.1 (docs)** — `testing-harness.md` shell component-closure enumeration now names `TaskListAppletRuntime`; the task-list applet row description covers the two-task dock fence, the arrow row, and the palette/import policy. `task-list.md` records the both-participants pending fence and the QST presentation boundary.

## Changed paths (sorted)

- docs/wiki/development/testing-harness.md
- docs/wiki/shell/task-list.md
- src/shell/task_list/applet/CMakeLists.txt
- src/shell/task_list/applet/include/qindaqt/shell/task_list/applet/task_list_applet_controller.h
- src/shell/task_list/applet/qml/TaskListApplet.qml
- src/shell/task_list/applet/qml/TaskListEntryButton.qml
- src/shell/task_list/applet/src/task_list_applet_controller_intents.cpp
- tests/shell/qml/imports/QindaQt/Shell/TaskList/TaskListApplet.qml
- tests/shell/task_list/CMakeLists.txt
- tests/shell/task_list/check_task_list_applet_boundary.cmake
- tests/shell/task_list/installed_task_list_consumer/CMakeLists.txt
- tests/shell/task_list/installed_task_list_cpp_consumer.cpp
- tests/shell/task_list/run_installed_task_list_applet.cmake
- tests/shell/task_list/tst_task_list_applet_operations.cpp
- tests/shell/task_list/tst_task_list_applet_qml.cpp

## Evidence (all run on this candidate; build root /home/cabewse/work_SPaC3/builds/qindaqt/task-list-applet)

- Focused build, Debug and Release, strict warnings-as-errors: `cmake --build <root>/<profile> --parallel 3 --target <task-list test targets + qindaqt-shell + applet manifest/catalog + shell-runtime-options>` — exit 0 both profiles.
- `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <profile> -R '^qindaqt\.(task-list-|applet|shell-runtime-)' --output-on-failure --no-tests=error` — **30/30 passed, Debug and Release** (includes the new arrows row, the repaired boundary probe, and the relocated installed-package proof). First Debug attempt reported qindaqt.shell-runtime-options Not Run; after building `qindaqt_shell_runtime_options_tests` the rerun is 30/30.
- `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <profile> -R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)' --output-on-failure --no-tests=error` — **2/2 passed, Debug and Release** (`desktop.virtual.sandbox-unit`, `desktop.virtual.package-contract`). No test named `desktop.virtual.stage-closure` exists in this tree (`ctest -N -R stage-closure` → Total Tests: 0); the selector is satisfied by the two configured rows.
- `./tools/validate-docs` — exit 0 (135 documents).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <root>/site` — exit 0.
- `./tools/check-source-shape` — exit 0 (2346 files; the moved dock slot keeps both touched test files under the 500-line review threshold and every function under 180 lines; pre-existing warnings in other lanes' files unchanged).
- `git diff --check` (da9f2fd..candidate) — exit 0.
- JSON: no JSON file changed in this repair, so no json.tool run was needed (the policy file was already validated for da9f2fd).
- Negative controls on da9f2fd: dock-fence row fails (qtest exit 1, detailed above); boundary probe rejects the da9f2fd applet QML (palette literal FATAL_ERROR).

## Bounded caveats (not claimed)

- No host desktop, host bus, hardware, uinput, or nested-compositor row was run; all evidence is offscreen/private-fake/staged-package.
- The applet remains a registered built-in, not hosted: production dispatcher composition is still the later lane; window-level activate/minimize/close still finish `Unavailable` through the T1 adapter until the window-actions composition lane.
- The context menu uses the QQC2 style palette because `QindaQt.Controls` ships no menu primitive; this is recorded in the wiki page, not hidden.

## Requested next action

Independent exact review by Lauren Williams (one recheck), then manager integration.
