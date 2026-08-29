# Tomas the 2nd — Terminal real-adapter defect repair handoff

- Time: 2026-08-28T19:43:04Z
- Exact candidate: `a9cc17f2e9a7edef78cac9e9fe7e2e5fb8410352`
- Tree: `905cf870e46ea541da0667d0eb67ab38d795b2cb`
- Parent: accepted source/build but live-rejected
  `bf195b6abfce978cdc51706b327dc7ac12823c73`
- Branch: `worker/terminal-s0-repair-tomas`
- Worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0-repair-tomas`
- Worktree state: clean
- Status: handoff; Tomas the 2nd is not live

## Delivered outcome

This one non-amended descendant repairs both Church the 3rd live findings
without changing any accepted PTY, lifecycle, teardown, dependency-confinement,
or input contract:

- generated unique atomic scheme paths now end in `.colorscheme`, the exact
  suffix qtermwidget 2.4 requires before it will load an existing custom file;
- bright ANSI sections now use qtermwidget's real
  `Color0Intense`..`Color7Intense` names instead of ignored `Color8`..`Color15`;
- LF/CR/Unicode-line-separator-only selections are treated as structural blank
  grid encoding, while spaces and tabs remain semantic copyable content;
- a registered test links the production adapter, renders actual qtermwidget
  `#171a18`, and proves pristine Select All publishes false; and
- the Terminal wiki records the upstream lookup/format contract and mandatory
  private live gate.

## Exact changed paths and manifests

1. `docs/wiki/apps/terminal.md`
2. `src/apps/terminal/ui/terminal_appearance.cpp`
3. `src/apps/terminal/ui/terminal_widget_adapter.cpp`
4. `tests/apps/terminal/CMakeLists.txt`
5. `tests/apps/terminal/tst_terminal_appearance.cpp`
6. `tests/apps/terminal/tst_terminal_widget_adapter.cpp` (new)

- Sorted name-status SHA-256:
  `06d08d2a60abc2c12ababe04fe9dc024b788a92d7b266f68c0522857c7343fc2`
- Sorted path-only SHA-256:
  `14c936c06593bb6e7812f66b439c31a9c379ecacae488986ddbd2054fbcae411`
- Binary commit-diff SHA-256:
  `999262e2d70c1fc1e31cf58ed048c3573120fa6046276b9f936dba9d52c457be`

## Executable and static evidence

All generated artifacts are under
`/mnt/d/QindaQt/builds/terminal-s0-live-repair-tomas2`. Debug and Release use
the extracted qtermwidget 2.4.0 prefix at
`/mnt/d/QindaQt/builds/terminal-s0-review-church/deps/qtermwidget-prefix`,
strict warnings, tests enabled, shell/production-shell/KWin/host-uinput off,
and `-DCMAKE_AUTOMOC_PATH_PREFIX=ON`.

- Debug configure and focused build: exit 0.
- Release configure and focused build: exit 0.
- Built in each: production support, adapter, executable, launch-policy,
  PTY-bridge, session, appearance, window, and real-adapter test targets.
- Complete display-variable-unset registered selector: **9/9** Debug and
  **9/9** Release, exit 0.
- Direct pure appearance executable: **7/7** Debug and **7/7** Release.
- Direct production-adapter executable: **4/4** Debug and **4/4** Release.
- The retained exact-parent Church harness is the negative control: it reports
  LF blank-selection false truth and white-vs-`#171a18` rendering failures.
  The new registered real-adapter row passes both assertions on this repair.
- `git diff --check`: exit 0.
- `tools/check-source-shape`: exit 0, 1031 files; adapter 496 nonblank lines,
  below the 500-line decomposition-review trigger.
- `tools/validate-docs`: exit 0, 66 documents/navigation.
- `/home/cabewse/venv/bin/mkdocs build --strict -d
  /mnt/d/QindaQt/builds/terminal-s0-live-repair-tomas2/site`: exit 0.
- Final commit/tree/parent and manifests repeat; `git status --porcelain` is
  empty. Process scan shows no build, test, Terminal child, or private Weston
  process left by this lane.

## Bounded caveats and requested next action

This implementer evidence is not acceptance. Dijkstra the 2nd must review the
exact immutable `a9cc17f` descendant, including the production-adapter test,
format groups, semantic-selection predicate, and all prior accepted behavior.
On source/build PASS, Church the 3rd must relink the same retained private
Weston harness against this candidate and rerun all 40 assertions, especially
both unselected frame captures and blank Select All. Only those independent
gates can close Church's `0/1/1/0` verdict and permit manager integration.

The first-party queue still names rejected `9bd5444`; its manager-owned row
needs reconciliation to this exact candidate after the Program Manager
preserves the commit. No product progress is claimed from this handoff alone.
