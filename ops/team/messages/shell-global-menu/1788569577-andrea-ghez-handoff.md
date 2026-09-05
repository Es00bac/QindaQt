# Handoff — first-party global-menu export for the Terminal and Text Editor (Andrea Ghez)

- Candidate commit: `e8e5170beb9849752341d1fec1ad47ffa5e6ca1e`
  (tree `3c79b361dfbcdd0e94a6d5f193d0bec1179ca2bb`)
- Exact base: `86ad7c3854a5e35efb0a3b3e65444c432448fa0e`
  (the runtime-repair candidate branch tip; its Terminal PTY/render files are
  untouched except the additive `main.cpp` composition and the two mechanical
  helper extractions needed to keep `main()` under the 180-line budget)
- Branch: `worker/first-party-menu-export`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/first-party-menu-export`

## User-visible outcome

The panel's global menu no longer shows **Menu unavailable** while the
QindaQt Terminal or the QindaQt Text Editor is focused: both applications now
compose the same AppShell `ApplicationMenuExport` the File Manager composes,
with the same lifecycle (composition after the primary window exists, exact
platform identity facts, withdrawal after an accepted close), the same
fail-closed behavior when the session bus or registrar is absent, and no
duplicated menu logic.

## What changed

1. `src/app_shell/menu_export` (additive): one shared fail-closed entry
   `composeFirstPartyMenuExport(coordinator, window, sessionBus)` returning a
   retained `unique_ptr<QObject>`. The bus is injected (the module-boundary
   rule and the `qindaqt.app-shell-source-policy` row forbid an ambient
   session-bus lookup there), a disconnected bus returns null, and the former
   File-Manager-only test seam (fixed `QINDAQT_TEST_APPMENU_WINDOW_ID` +
   `QINDAQT_TEST_APPMENU_TRACE_ACTIVATION` trace) moved into it behind a
   `BUILD_TESTING`-only `QINDAQT_ENABLE_APP_MENU_EXPORT_TEST_SEAM`.
2. File Manager (bounded deviation, directed by the lane's "route all three
   apps through it"): `composeFileManagerMenuExport` now delegates to the
   shared entry; env-var seam, behavior, and the existing
   `qindaqt.file-manager-global-menu-shell-private-bus` rows are preserved
   (rerun green in both configs).
3. Terminal: public `TerminalWindow::appShellCoordinator()` seam;
   `composeTerminalMenuExport(window)` helper called in `main()` right after
   `window.show()`; the export object is retained for the window lifetime and
   destroyed before the window.
4. Text Editor: same composition in `main()` after `window.show()`, using the
   existing `EditorWindow::appShellCoordinator()`.
5. Tests (mirroring the File Manager rows, per app):
   - `qindaqt.(terminal|editor)-global-menu-shell-private-bus` — real process
     offscreen + production `GlobalMenuAppletComposition` on one
     `dbus-run-session` bus; exact child PID + window id 77 → applet
     available with the app's real menu tree, one shell activation counted
     exactly once, then `file.quit` activated through the shell menu ends the
     provider through the real close path and the applet clears; mismatched
     PID and mismatched window-id variants stay unavailable/empty with zero
     activations while the child stays alive.
   - `qindaqt.(terminal|editor)-global-menu-registrar-absent-private-bus` —
     the application keeps running quietly with no registrar on the bus and
     binds only when the production registrar later appears.
   - `qindaqt.(terminal|editor)-global-menu-hostile-registrar-private-bus` —
     a hostile registrar owner refuses `RegisterWindow` with a D-Bus error;
     the application must stay alive and rebind once the real registrar owns
     the name.
   - `qindaqt.(terminal|editor)-global-menu-identity-variants-source-policy`
     registered rows keep the hostile variants and the live child-PID
     boundary in the test graph (P2-01-style regression proof).
   - Note: the Terminal rows deliberately run the real process without
     `QT_FATAL_WARNINGS` because qtermwidget under the offscreen QPA emits
     "This plugin does not support propagateSizeHints()", which would abort
     the process before any menu evidence; the existing qtermwidget-linked
     rows share that convention. The exact stderr allowlist is asserted.
6. `tests/app_shell/CMakeLists.txt` (owned path, pre-existing defect fix): it
   consumed `QINDAQT_DBUS_RUN_SESSION` that only `tests/session` (a later
   configure directory) defines, so a fresh build tree registered
   `qindaqt.app-shell-menu-export-private-bus` with an empty command until a
   second configure. It now runs `find_program(... REQUIRED)` itself.
7. Docs: `docs/wiki/shell/global-menu.md` (first-party consumers, foreign-
   toolkit boundary wording, Remaining boundaries, verification rows), both
   app pages, the testing-harness selector and row descriptions, and the
   `src/app_shell/menu_export` module-boundaries row. No new pages, so
   `mkdocs.yml` navigation is unchanged.

## Changed paths (sorted)

- docs/wiki/apps/terminal.md
- docs/wiki/apps/text-editor.md
- docs/wiki/architecture/module-boundaries.md
- docs/wiki/development/testing-harness.md
- docs/wiki/shell/global-menu.md
- src/app_shell/menu_export/CMakeLists.txt
- src/app_shell/menu_export/include/qindaqt/app_shell/menu_export/first_party_composition.h (new)
- src/app_shell/menu_export/src/first_party_composition.cpp (new)
- src/apps/file_manager/CMakeLists.txt
- src/apps/file_manager/app_shell/file_manager_action_catalog.cpp
- src/apps/terminal/CMakeLists.txt
- src/apps/terminal/main.cpp
- src/apps/terminal/ui/terminal_window.cpp
- src/apps/terminal/ui/terminal_window.h
- src/apps/text_editor/CMakeLists.txt
- src/apps/text_editor/main.cpp
- tests/app_shell/CMakeLists.txt
- tests/apps/terminal/CMakeLists.txt
- tests/apps/terminal/check_menu_export_identity_variants.cmake (new)
- tests/apps/terminal/tst_terminal_menu_export.cpp (new)
- tests/apps/text_editor/CMakeLists.txt
- tests/apps/text_editor/check_menu_export_identity_variants.cmake (new)
- tests/apps/text_editor/tst_editor_menu_export.cpp (new)

## Evidence (all actually run on this worktree)

Configure (both exit 0):

    cmake -S . -B <ROOT>/{debug,release} -G Ninja \
      -C .../qindaqt-system-kwin-initial-cache.cmake \
      -DCMAKE_BUILD_TYPE={Debug,Release} -DBUILD_TESTING=ON \
      -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
      -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
      -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
      -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

Focused builds (exit 0, warnings-as-errors): `qindaqt-terminal`,
`qindaqt-editor`, `qindaqt-file-manager`, both new test executables, the full
selector test closure, and the QML plugins in both configs (Debug 899 + plugin
targets; Release 899 + plugin targets).

Verification selector, run under
`env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`:

    ctest --test-dir <ROOT>/{debug,release} \
      -R '^qindaqt\.(terminal|editor|text-editor|app-shell|global-menu-|file-manager)' \
      --output-on-failure --no-tests=error

- Debug: **100% tests passed, 0 failed out of 89** (38.80 s)
- Release: **100% tests passed, 0 failed out of 89** (35.83 s)

New rows individually observed green in both configs (8 rows: 3 real-process
private-bus + 1 source-policy per app).

Static gates (run from the worktree root, all exit 0):

- `./tools/validate-docs` — validated 144 Markdown documents + navigation.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build
  --strict --site-dir <ROOT>/site` — built clean.
- `./tools/check-source-shape` — 0 errors (pre-existing review-threshold
  warnings on untouched files remain, as on the base).
- `git diff --check` — clean.
- No JSON files changed.

## Bounded caveats (what this candidate deliberately does not claim)

- No native Wayland/KDE-appmenu-hook qualification: the new rows prove the
  XWayland-shaped identity join through the test seam, exactly like the
  accepted File Manager row; Wayland announcement behavior remains covered by
  the module-level `qindaqt.app-shell-menu-export-private-bus` row.
- No installed nested-session or host-session qualification; private-bus
  evidence only.
- No foreign-toolkit (GTK) exporter.
- The two bounded path deviations from my owned list are named above (File
  Manager delegation + the tests/app_shell find_program fix); the manager
  should review those two hunks specifically.
- `check-source-shape` still prints decomposition-review warnings for
  pre-existing files outside this change (unchanged from the base).

## Requested next action

Independent exact review of `e8e5170beb9849752341d1fec1ad47ffa5e6ca1e`, then
manager integration.
