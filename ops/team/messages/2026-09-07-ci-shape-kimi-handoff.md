# Handoff: source-shape gate repair candidate

2026-09-07T12:38:00-06:00 — ci-shape-kimi

Candidate commit: `d4f65d840198dad038ec8ec125516f0c237ac1e3` on
`worker/ci-source-shape`, exact base `a258f17d`, worktree
`/home/cabewse/work_SPaC3/container-wm/.cache/ci-source-shape` (clean tree;
the checker ran against exactly this commit).

Changed paths:

- `src/shell/global_menu/applet/qml/GlobalMenuApplet.qml` (378 → 335
  nonblank): action-entry delegate extracted.
- `src/shell/global_menu/applet/qml/GlobalMenuActionEntry.qml` (new, 71
  nonblank): the extracted delegate; geometry/measurement stay applet-owned
  via function properties. Behavior-preserving.
- `src/shell/global_menu/applet/CMakeLists.txt`: new component added to both
  `QML_FILES` lists and the runtime install `FILES`.
- `tests/shell/global_menu/run_installed_global_menu.cmake`: staged-package
  contract now also requires the new installed QML source.
- `tests/session/CMakeLists.txt` (605 → 553 nonblank): Gabbee rows moved
  verbatim to the new include.
- `tests/session/GabbeeTests.cmake` (new, 54 nonblank): identical test
  names/commands/properties (`session.gabbee-interop-unit`,
  `session.gabbee-probe-syntax`, `session.gabbee-terminal-pty-unit`).
- `tests/session/gabbee/gabbee_terminal_boot.py`: `_run_inner_phases` 140 →
  100 lines; new `_start_private_bus` / `_start_private_pipewire` helpers
  carry their ordering guards. No behavior change.
- `docs/wiki/shell/global-menu.md`: names the extracted component.
- `ops/team/workers/ci-shape-kimi.md`, `ops/team/messages/` claim +
  root's exact-errors note.

Verification (this worktree, build/dev, max -j2, no nested/host launches):

- `./tools/check-source-shape`: exit 0, 2892 files, 0 errors (was 3).
- Gabbee Python units, direct: 26/26 interop, 21/21 PTY (2 documented
  real-sink skips), exit 0; `py_compile` clean.
- ctest `session.gabbee-*`: 3/3 pass, exit 0.
- Global Menu QML module `qindaqt_global_menu_qml` rebuilds clean; focused
  qmltestrunner rows 8/8 pass under `QT_FATAL_WARNINGS=1`
  (applet 14/14, overflow, vertical-boundary, accessibility, native-switch,
  native-submenu, hit-targets, production-panel-keyboard), exit 0.
- `qmllint` clean on both touched QML files (the now-unused
  `QtQuick.Controls` import was removed from the applet).
- `python3 tools/validate-docs`: 197 documents, exit 0;
  `mkdocs build --strict` (handbook-docs-venv): exit 0.
- `git diff --check`: clean.

Caveats: the installed-package row `qindaqt.global-menu-installed-package`
was not run (needs a full shell build; its contract script was updated
additively). Nested/host-launch rows are out of scope per assignment.
Decomposition-review warnings elsewhere in the tree are pre-existing and
untouched, per root's scoping note.

Requested next action: root reviews the exact candidate and integrates; the
CI source-shape job should then run the real native build jobs instead of
failing at the shape gate.
