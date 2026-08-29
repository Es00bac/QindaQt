# Dijkstra the 2nd — exact Terminal live-repair source/build PASS

- Time: 2026-08-28T19:58:05Z
- Exact candidate: `a9cc17f2e9a7edef78cac9e9fe7e2e5fb8410352`
- Tree: `905cf870e46ea541da0667d0eb67ab38d795b2cb`
- Parent: `bf195b6abfce978cdc51706b327dc7ac12823c73`
- Verdict: **PASS — P0/P1/P2/P3 = `0/0/0/0`**
- Product edits: none
- Reviewer state: handoff; Dijkstra the 2nd is not live

This exact non-amended repair independently passes the complete bounded
source/build rereview. It closes both findings in Church the 3rd's `0/1/1/0`
live FAIL at the source and production-adapter regression level without
reopening accepted PTY, lifecycle, teardown, packaging, or dependency
boundaries. This is deliberately **not integration acceptance**: the retained
private-Weston 40-assertion rerun remains mandatory.

## Prior live findings closed directly

1. **P1 theme application:** the atomic target now ends in `.colorscheme`,
   qtermwidget 2.4's accepted custom-scheme suffix. The generated document uses
   upstream's `Color0Intense` through `Color7Intense` groups and a bounded
   `[General]` section. The production adapter test paints the real widget and
   observes center pixel `#171a18`, exactly Qinda dark's derived terminal
   background. The upstream-installed Breeze document and library strings
   independently expose the same intense group names and `*.colorscheme`.
2. **P2 blank selection truth:** the adapter now treats only LF, CR, U+2028,
   and U+2029 as structural row separators; ordinary spaces and tabs still
   take the semantic-true branch. A pristine real qtermwidget Select All emits
   one availability signal with `false`, and `hasSelectedText()` remains false.

The hostile control compiles the candidate's tests against the three restored
parent behaviors. Its appearance executable exits 1 at missing
`[Color0Intense]` (6 passed/1 failed). Its real-adapter executable exits 2
(2 passed/2 failed): the `.ini` loader logs that it cannot load the custom
scheme and renders white `0xffffffff` instead of `#171a18`; `!isEmpty()`
publishes true for the pristine LF selection. These failures prove the new
rows are sensitive to every repaired behavior rather than merely executable.

## Independent compiled and package evidence

All generated build/test output is under
`/mnt/d/QindaQt/builds/terminal-s0-colors-rereview-dijkstra`. Both Debug and
Release configure with tests enabled, shell/production-shell/KWin/host-uinput
disabled, strict warnings enabled, the pinned qtermwidget prefix, and
`-DCMAKE_AUTOMOC_PATH_PREFIX=ON`.

- Debug configure: exit 0; focused 63-step build: exit 0.
- Release configure: exit 0; focused 63-step build: exit 0.
- Each build includes Terminal support, production adapter, executable,
  launch-policy, PTY-bridge, session, appearance, window, and production-
  adapter test targets.
- With `DISPLAY`, `WAYLAND_DISPLAY`, `XDG_RUNTIME_DIR`, and caller
  `QT_QPA_PLATFORM` unset, `ctest -R '^qindaqt\.terminal-'`: **9/9** Debug and
  **9/9** Release, exit 0.
- Direct appearance: **7/7** Debug and **7/7** Release, exit 0.
- Direct production adapter: **4/4** Debug and **4/4** Release, exit 0. The
  repeated final run redirects XDG cache under the `/mnt/d` evidence root and
  leaves no generated scheme file behind.
- Installed `Terminal` component/theme probe: **1/1** Debug and **1/1**
  Release, exit 0. It stages the executable, desktop entry, and theme; strips
  ambient HOME/XDG theme roots; resolves `qinda-dark qst-1`; then removes the
  stage.
- A synthetic qtermwidget 2.5.0 package is rejected at configure with exit 1
  as incompatible with `2.4...<2.5`; its poison config body is never loaded.
- The real library is the extracted qtermwidget 2.4.0 binary with SHA-256
  `b1440218096965e6161d67fab56d5f4ef6da869ad02cdb8999e98aa95a990dd1`.
  `ldd` resolves the app and the one production-adapter test to that exact
  prefix. Build rules expose qtermwidget headers only to the adapter compile;
  dynamic linkage reaches only the app and that deliberate real-adapter test.

## Static, documentation, identity, and cleanliness gates

- `git diff --check <parent>..HEAD` and worktree `git diff --check`: exit 0.
- `python3 tools/check-source-shape`: exit 0, 1031 source files; production
  adapter 496 nonblank lines, below the 500-line decomposition-review trigger.
- `python3 tools/validate-docs`: exit 0, 66 documents/navigation.
- `/home/cabewse/venv/bin/mkdocs build --strict -d <evidence>/site`: exit 0.
- Exact six-path sorted name-status SHA-256:
  `06d08d2a60abc2c12ababe04fe9dc024b788a92d7b266f68c0522857c7343fc2`.
- Exact sorted path-only SHA-256:
  `14c936c06593bb6e7812f66b439c31a9c379ecacae488986ddbd2054fbcae411`.
- Exact binary commit-diff SHA-256:
  `999262e2d70c1fc1e31cf58ed048c3573120fa6046276b9f936dba9d52c457be`.
- Final commit/tree/parent repeat exactly; detached candidate worktree has an
  empty porcelain status and empty uncommitted diff. No build, test, Terminal,
  child, or Weston process remains from this lane.

No host desktop, compositor, input device, session, cursor, or configuration
was used or changed.

## Required next action

Church the 3rd must now relink the retained private-Weston qualifier to exact
commit `a9cc17f2e9a7edef78cac9e9fe7e2e5fb8410352` and rerun **all 40 assertions**,
including the unselected first/populated frame captures and pristine blank
Select All. Only a Church exact live PASS can authorize manager integration.
