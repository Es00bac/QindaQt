# Claim: source-shape gate repair (three exact violations)

2026-09-07T12:21:00-06:00 — ci-shape-kimi

Claiming the bounded repair of the three `./tools/check-source-shape` errors
on exact base `a258f17d`, worktree
`/home/cabewse/work_SPaC3/container-wm/.cache/ci-source-shape`, branch
`worker/ci-source-shape`:

- `src/shell/global_menu/applet/qml/GlobalMenuApplet.qml` (378 > 350):
  extract the top-level action-entry delegate into a sibling
  `GlobalMenuActionEntry.qml` component; geometry/measurement functions stay
  applet-owned and are passed in as properties. Registered in the applet
  QML module lists and install set.
- `tests/session/CMakeLists.txt` (605 > 600): move the Gabbee interop/PTY
  test registrations into `tests/session/GabbeeTests.cmake`, matching the
  existing `DesktopSessionTests.cmake`/`PanelVisibilityTests.cmake` include
  pattern. Zero test-name or property changes.
- `tests/session/gabbee/gabbee_terminal_boot.py` (`_run_inner_phases` 140 >
  120 at line 116): split the private-bus and private-PipeWire phases into
  cohesive helpers; ordering guards stay at the call sites.

No checker weakening, no exemptions, no reformat-to-game-counts. Owned paths
are the three files plus their minimal registrations and
`ops/team/workers/ci-shape-kimi.md`. Verification: source-shape gate,
Gabbee Python unit rows, focused Global Menu QML build/tests as available,
strict MkDocs and the link checker. Candidate handoff will name the exact
commit, changed paths, and test counts/exits.
