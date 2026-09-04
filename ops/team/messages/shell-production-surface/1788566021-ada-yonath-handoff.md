# Ada Yonath — production shell runtime repair handoff

## Candidate

- Candidate commit: `99de545aa521c4d5428b72ee491268e401a5b84c`
- Candidate tree: `de8b32ee719b7f6509aa2ec172df40e8bac8b7af`
- Exact base: `dd415f48f3a12ab95db9ca26b7227a1e0bcf69af`
- Branch: `worker/shell-production-runtime-repair`
- Requested next action: **independent exact review then manager integration**.

## Symptom → cause → fix → regression evidence

1. Bare white/text-only panels and tens of thousands of QML warnings:
   - Cause: neither shell composition root published its process-local `QindaQt.Tokens` facade before creating Controls/hosted applets. The exact-base installed preview completed with 33,768 `Unable to assign [undefined]` messages; fatal warnings abort it with exit 134.
   - Fix: `ShellTokenPublisher` publishes the selected `ThemeCatalog` entry before dispatcher QML, atomically republishes on theme changes, and makes initial or later publication failure fatal. Production and preview share this boundary. Preview and production installed RUNPATHs resolve the sibling Tokens backing library. The unrelated delegate warning that preceded token warnings under fatal mode was repaired by declaring the required `index` role in `ContainerTabStrip`.
   - Row: `qindaqt.shell-runtime-token-publication` asserts `Tokens.ready`, QST revision/theme, concrete `bg.base`, a hosted Task List Controls foreground, and theme republish under `QT_FATAL_WARNINGS=1`. `qindaqt.shell-capture-matrix` makes the complete dispatcher warning-fatal. Exact-base negative preview exits 134; repaired Debug and Release rows pass.

2. No machine-readable guard that the live shell actually published tokens:
   - Cause: screenshots could look plausible without proving engine-local singleton readiness or exact shell ownership.
   - Fix: authenticated development-only `ShellDevelopment1.Snapshot` carries an exact five-field `tokens` fact. C++ and Python consumers reject missing/extra fields, unready state, wrong QST revision, non-positive/non-canonical generation, empty theme, or a non-canonical lowercase RGB value.
   - Row: `desktop.virtual.boot.1080p` run `e7efa02f5cc0a8e40ea61f0c11126aed` authenticated owner/PID `24` and reported `{ready:true,qstRevision:1,generation:"1",sourceThemeId:"qinda-dark",backgroundBase:"#171a18"}`. `desktop.virtual.notification-shell-readiness-unit` covers hostile mutations.

3. Task List says **Limited**:
   - Cause: not an exact-owner, credential, virtual-backend, or timing failure. The live compositor publishes `Windows` schema 2 and all committed dock surfaces under the authenticated shell PID, but current Compositor1 has no single atomic task-fact generation joining window, output, workspace, role, and container lineage. T1 deliberately degrades rather than combining independent snapshots.
   - Fix: no unsafe placeholder was introduced. The exact requirement is documented on the owning page. Removing **Limited** remains a separately owned compositor contract outcome.
   - Row: the successful virtual boot evidence contains coherent exact-owner `DevelopmentShellSurfaces`, `ShellVisibilitySnapshot`, and `Windows` generations; the 23 focused Task List rows remain green.

4. Global Menu says **Menu unavailable** for Terminal:
   - Cause: Terminal owns a local `QMenuBar` but does not opt into the first-party `ApplicationMenuExport` composition. Registrar presence and an active Terminal window do not constitute a native Wayland appmenu address. This is independent of virtual/windowed/DRM compositor backend.
   - Fix: no fabricated menu or compositor workaround was added. The owning page now states the exact requirement: Terminal must compose the AppShell exporter, or qualification must prove a KDE platform exporter plus non-empty authenticated service/path identity facts.
   - Row: all focused Global Menu ownership, transport, private-bus, hostile, and production-panel rows pass. This candidate deliberately does not claim Terminal menu export.

5. Terminal has running bash children but an empty viewport:
   - Cause A: Linux returns `EIO` on the PTY master while no slave is open. The GUI notifier could consume that transient gap before the child opened its controlling tty and permanently disable output forwarding.
   - Fix A: the bridge retains a close-on-exec, non-controlling slave guard until teardown; child reap remains exit authority.
   - Row A: the deterministic `qindaqt.terminal-pty-bridge` transient-slave gap test fails against exact base with the read notifier disabled (CTest exit 8, 7 passed/1 failed) and passes in Debug and Release.
   - Cause B: qtermwidget entered teletype mode while parentless, before the production window attached and sized it. Fast child output could enter searchable scrollback while the later live reparent/resize left glyph painting blank.
   - Fix B: `TerminalSession` publishes/attaches the backend widget before `start()`, the adapter starts and primes its rendering teletype only afterward, synchronously supplies the attached resize, and `main()` realizes the top-level before scheduling the first session. The rendering transport remains byte-transparent and bounded.
   - Row B: the updated `qindaqt.terminal-session` row fails against exact base because the widget is published after backend start (CTest exit 8, 15 passed/2 failed) and passes repaired Debug and Release. `qindaqt.terminal-widget-adapter-offscreen` additionally proves real child output produces foreground glyph pixels inside qtermwidget itself; its exact-base offscreen pixel row passes, so it is supplementary visual coverage rather than the negative control.

6. Remaining usability sweep:
   - The focused launcher, clock/dispatcher capture, notification-center, panel visibility/reveal, Settings launch/package, status notifier, clipboard, Task List, Global Menu, and Terminal rows are green. Both serial virtual panel-visibility rows pass with real private-seat input/capture. No additional `src/shell/**` defect was found.
   - Network, Bluetooth, and power availability were not claimed because the required recipe blocks the system bus and supplies unavailable power upstream.

## Changed paths (sorted; candidate tree)

- `docs/wiki/adr/0071-publish-and-prove-shell-token-readiness.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/design-tokens.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/compositor-control-v1.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/controls.md`
- `docs/wiki/shell/global-menu.md`
- `docs/wiki/shell/panel-surfaces.md`
- `docs/wiki/shell/task-list.md`
- `mkdocs.yml`
- `src/apps/terminal/CMakeLists.txt`
- `src/apps/terminal/main.cpp`
- `src/apps/terminal/session/pty_bridge.cpp`
- `src/apps/terminal/session/pty_bridge.h`
- `src/apps/terminal/session/terminal_session.cpp`
- `src/apps/terminal/session/terminal_session_backend.h`
- `src/apps/terminal/ui/terminal_widget_adapter.cpp`
- `src/apps/terminal/ui/terminal_widget_adapter.h`
- `src/apps/terminal/ui/terminal_widget_adapter_transport.cpp`
- `src/shell/CMakeLists.txt`
- `src/shell/app/shellpreviewapplication.cpp`
- `src/shell/app/shellpreviewapplication.h`
- `src/shell/common/shelltokenpublisher.cpp`
- `src/shell/common/shelltokenpublisher.h`
- `src/shell/qml/ContainerTabStrip.qml`
- `src/shell/runtime/shelldevelopmentevidence.cpp`
- `src/shell/runtime/shelldevelopmentevidence.h`
- `src/shell/runtime/shellruntimeapplication.cpp`
- `src/shell/runtime/shellruntimeapplication.h`
- `src/shell/runtime/shellruntimeapplication_applets.cpp`
- `src/shell/runtime/shellruntimeapplication_development.cpp`
- `src/shell/runtime/shellruntimeapplication_tokens.cpp`
- `tests/apps/terminal/tst_pty_bridge.cpp`
- `tests/apps/terminal/tst_terminal_session.cpp`
- `tests/apps/terminal/tst_terminal_widget_adapter.cpp`
- `tests/session/DesktopNotificationShellReadinessTests.cmake`
- `tests/session/DesktopSessionTests.cmake`
- `tests/session/desktop_session_interactive.py`
- `tests/session/desktop_session_notification_shell.py`
- `tests/session/desktop_session_shell_fixtures.py`
- `tests/session/desktopnotificationshellreadiness.cpp`
- `tests/session/desktopnotificationshellsample.cpp`
- `tests/session/fixtures/desktop_session/probe-observed-fallback-1080p.json`
- `tests/session/fixtures/desktop_session/probe-ready-1080p.json`
- `tests/session/test_desktop_session_interactive_unit.py`
- `tests/session/test_desktop_session_matrix_unit.py`
- `tests/session/tst_desktopnotificationshellreadiness.cpp`
- `tests/shell/CMakeLists.txt`
- `tests/shell/tst_shell_capture.cpp`
- `tests/shell/tst_shellruntime_tokens.cpp`

## Verification evidence

All display variables were unset and `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`; product/session tests used no host bus, host display, uinput, hardware, or network.

- Exact prescribed Debug configure: exit 0.
- Exact prescribed Release configure: exit 0.
- Focused owned/dependency-adjacent targets in Debug and Release (`qindaqt-shell`, `qindaqt-shell-preview`, shell-token test, Terminal tests/application, notification-shell readiness test, desktop probe): exit 0 in both configurations.
- `ctest -R '^qindaqt\.(shell-runtime-|applet|task-list-|global-menu-|status-notifier-|clipboard-applet-|launcher|notification-center|terminal)' ...`: Debug 131/131 passed, exit 0; Release 131/131 passed, exit 0.
- `ctest -R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)' ...`: Debug 3/3 passed, exit 0; Release 3/3 passed, exit 0.
- `ctest -R '^(desktop\.virtual\.notification-shell-readiness-unit|qindaqt\.shell-capture-matrix)$' ...`: Debug 2/2 passed, exit 0; Release 2/2 passed, exit 0.
- `ctest -R '^(qindaqt\.shell-capture-matrix|qindaqt\.shell-runtime-component-closure)$' ...` after the preview RUNPATH fix: Debug 2/2 passed, exit 0; Release 2/2 passed, exit 0.
- `QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop ctest -R '^desktop\.virtual\.boot\.1080p$' --parallel 1 ...`: 2/2 including package dependency passed, exit 0; run `e7efa02f5cc0a8e40ea61f0c11126aed`, contained runtime 4.889 s.
- Same private virtual lane, `ctest -R '^desktop\.virtual\.panel-visibility\.single-1080p$' --parallel 1 ...`: 2/2 passed, exit 0; run `474439856e0880304a12f51ce449fd42`, contained runtime 44.026 s.
- Same private virtual lane, `ctest -R '^desktop\.virtual\.panel-visibility\.single-wuxga$' --parallel 1 ...`: 2/2 passed, exit 0; run `032ac8bc9f8b9af654d40699e133a51a`, contained runtime 47.950 s.
- A first virtual boot attempt before the successful bounded retry failed because no dock mapped before its deadline; cleanup evidence reported no survivors. The successful repeat above is the claimed boot evidence. No panel row was retried.
- Final `pgrep -af 'kwin_wayland.*qindaqt-parent-way[l]and'`: exit 1/no output (no private compositor survivor).
- `cmake --install .../release --prefix .../prefix`: the initial full-tree traversal exposed previously unbuilt unrelated portal, network-secret-agent, and file-manager install targets (three exit-1 attempts). Building only those named missing targets allowed the final identical install command to exit 0. No default whole-repository build was run.
- Installed preview direct smoke with display/session-bus absent and `QT_FATAL_WARNINGS=1`: exit 0; emitted a valid 1920×1080 PNG and its ELF RUNPATH includes `$ORIGIN/../Tokens`.
- Exact-base negative dispatcher: ordinary run exit 0 with 33,768 undefined assignments; warning-fatal run exit 134 on the first dispatcher QML warning.
- Exact-base negative Terminal rows compiled from base `dd415f48...` plus tests only: `qindaqt.terminal-pty-bridge` exit 8 (7 passed/1 failed); `qindaqt.terminal-session` exit 8 (15 passed/2 failed). The repaired counterparts pass in both configurations.
- `./tools/validate-docs`: exit 0, 144 Markdown documents plus navigation validated.
- `mkdocs build --strict --site-dir .../site`: exit 0.
- `./tools/check-source-shape`: exit 0, 2,546 files checked, zero allowlisted/skipped. Only pre-existing decomposition-review warnings plus the still-below-hard-limit shell CMake file were reported; changed Terminal test sources are below 500.
- `git diff --check`: exit 0.
- `python3 -m json.tool` for both changed JSON fixtures: exit 0.

## Screenshots and bounded caveats

- Supplied before image: `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/shell-production-runtime-repair/user-screenshot.png`.
- Preserved pre-stop windowed panel-after artifact: `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/shell-production-runtime-repair/after-repair.png`. It predates the final Terminal ordering/PTY verification and is not claimed as final Terminal proof.
- Final safe after image: `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/shell-production-runtime-repair/after-headless-offscreen.png`; generated from the final installed preview without host display/session bus under fatal warnings. The supplied launcher script was converted to virtual-headless paths under the assigned build root; no new windowed run was performed after the stop instruction.
- This candidate does not claim system-bus-backed power/network/Bluetooth availability, Terminal Global Menu export, a live compositor task-fact contract, hardware, host desktop, or a new windowed screenshot. It does claim warning-clean styled shell composition, authenticated live token readiness, corrected Terminal PTY/viewport ordering, and the explicitly executed private virtual rows above.
