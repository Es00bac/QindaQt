# Native DPMS qualification preparation / narrow shortcut source acceptance

- Timestamp:2026-10-02T20:05:26Z
- Root coherent failed source:69817215fb6895ca963bca05d2263960fc0c5c5c.
- Separate unchanged own runtime WT: qindaqt-kwin.worktrees/pf-native-display-power-runtime-20261002 / worker/pf-native-display-power-runtime-20261002 at exact698; prior10c/2b09 worktree/refs preserved.
- Root build log SHA256:315251e716f20f35292e04b4485e3b3656898cc7d99a109c2e27c836530391a8.
- Actual log finding: testNativeShortcuts native_lock_fixture.h line10 cannot find sessionlockcontroller.h. DisplayPower native_display_power_test.cpp compiled successfully at438/450; unchanged virtual_output.cpp also compiled, with upstream initializer warnings. No DPMS source repair is indicated by this build log.

Root implemented the only missing PRIVATE include directory. Independent exact
source ACCEPT5a21df52de1311ab061c5ed26665ea1a552868a8: one CMake file,
existing qindaqt/session-lock include directory added to testNativeShortcuts;
its required header exists and testNativeSessionLock already uses identical
boundary at line266. Diffcheck0. Existing generated protocol sharing/guards and
production admission/source remain unchanged. No compilation/runtime performed
by this reviewer, and first root failure is retained rather than called a pass.

Root confirmed the intended DPMS runtime is the exact virtual/software Workspace
authority gate. Framework forces KWIN_COMPOSE=Q; run-native-lock forces software
GL. No fixture compositor-mode change or hardware/GPU-rendering claim is needed.
The11 behavior rows/13expectedQt lifecycle-inclusive checks remain authored,
not observed. Root owns coherent shared-core/source/cache build; worker waits for
exact artifact freeze and private runtime lease before running the gate. No
manager warm source/cache or production/host state has been changed. Tested
818/869 startup rejection/off-lifecycle receipt remains preserved and separate;
complete supported supervisor composition is still unqualified.

Status available waiting after read-only finding/source acceptance; no compiler
or private runtime lease held. Next bounded action is one actual coherent
private testNativeDisplayPower run and truthful first outcome/cleanup receipt,
without physical DPMS/sleep or game/icon work.
