# Installed and retained-keyboard repair handoff

- Timestamp: 2026-09-27T11:14:38-06:00
- Exact repaired candidate: `292cd361adb15f9d3ace5a1cb2f4a3e277e03238`
- Branch: `fix/shell-icon-performance`, pushed to qinda hub
- Same worktree: `/home/cabewse/work_space/container-wm-performance-worker`
- Requested action: same-reviewer recheck, manager integration, staged/relocated installed consumer and exact keyboard gate on native built tree.

Production repair `e37f01f9` explicitly imports `QindaQt.Shell.TaskList 1.0`
under an alias in TaskListApplet.qml and uses its exported KeyedRowModel. This
makes direct-file staged loading activate real module C++ registration; no
manual test registration is introduced. Same reviewer accepted that commit.

The keyboard failure is an offscreen focus lifecycle artifact: the retained
popup emits closed but QGuiApplication::focusWindow remains its hidden native
window. The host's isActive flag alone is insufficient. The repaired fixture
waits closed, requests parent activation and waits for the exact focusWindow,
then asserts the row holds active focus before delivering the next synthetic
Menu key. It also proves the same context menu object survives publication.
Production focus behavior is unchanged (no focus-stealing workaround).

Changed repair paths: TaskListApplet.qml; tst_task_list_applet_qml.cpp;
TaskList wiki and own worker record. `cmake --build build/performance --parallel
4 --target qindaqt_task_list_applet_qml_tests` and the five original focused
targets: exit 0. Final six-row CTest selection (keyboard-offscreen, launcher
persistence, keyed-row-model, task dock, QuickLaunch dock, retained delegates):
exit 0, **6/6**, 5.73 seconds; keyboard 0.37 seconds. Strict MkDocs and
repository docs links: exit 0, 417 documents. Diff check: exit 0.

The local installed package gate stopped before consumer execution because
TaskListAppletRuntime component installation also needs the unbuilt full shell
executable. Root's native combined tree already has that executable and owns
the exact original/relocated-stage proof. No installed success is claimed here.

Available for the exact reviewer or integrated gate repair; no live session
or shared native build mutation performed by this worker.
