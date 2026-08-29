# Noether the 2nd — Terminal manager-base replay handoff

- Time: 2026-08-28T14:25:37-06:00
- Exact candidate: `82830b96f29a916b2711de260269c67d2a9b59d9`
- Tree: `a3f7bbdb96aa08aa7bc5a87da02d50cf5b49c343`
- Parent: `77f335e9532d9c12946f40987e62a5c18c2ef42e`
- Exact manager base: `d0e08095dca9b09b1125f994f784be659ce68f65`
- Branch: `worker/terminal-manager-replay-noether2`
- Worktree: `/mnt/d/QindaQt/worktrees/terminal-manager-replay-noether2`
- Status: handoff; Noether the 2nd is not live
- Requested action: different-worker exact replay review, then manager integration

## Outcome

The accepted Terminal S0 lineage is now a clean seven-commit series descending
from the exact current manager boundary. The first six commits preserve the
original authorship, subjects, and order:

1. `79b3732c5dfbca664bd768467a23a97375c4df17` — original `a15a5f2`
2. `8d887137d45247f1df22ab45a13c572065e7b2e7` — original `f98d0e1`
3. `75e825a5389459a43582222e6d8c2b04e948cf6a` — original `2386e74`
4. `bf4fdaf992dd08a559ea1b3a82cebfeedf072349` — original `9bd5444`
5. `6c17853918e69c2c69994664d48b9d2550976627` — original `bf195b6`
6. `77f335e9532d9c12946f40987e62a5c18c2ef42e` — original `a9cc17f`
7. `82830b96f29a916b2711de260269c67d2a9b59d9` — manager-union package repair

Production `src/apps/terminal/**` is byte-identical to independently accepted
`a9cc17f`. The seventh commit changes only the Terminal wiki verification
paragraph and installed-metadata test boundary. It passes the exact imported
qtermwidget library file to the clean staged probe, validates the file, strips
ambient `LD_LIBRARY_PATH`, and uses only the imported file's directory. This
closes the reproduced loader failure caused when current manager policy
correctly replaced the candidate's implicit build-prefix RPATH with relocatable
`$ORIGIN` paths. It does not alter production or private-live bytes.

ADR-0030 is retained as superseded history because its confinement, version
pinning, exit-truth, and teardown contracts remain in force. Collision-free
accepted ADR-0040 supersedes only the invalid slave-forwarding design with the
application-owned child-PTY bridge.

## Manual conflict resolutions

All manual resolutions were strict additive unions; no Terminal production or
focused-test hunk needed conflict editing:

- first replay commit: `docs/wiki/adr/index.md`,
  `docs/wiki/architecture/module-boundaries.md`, `docs/wiki/index.md`,
  `mkdocs.yml`, `src/CMakeLists.txt`, and `tests/CMakeLists.txt`;
- second replay commit: ADR index status update while preserving every manager row;
- third replay commit: ADR-0030 rename in ADR index, module boundary, and MkDocs nav;
- fourth replay commit: ADR-0030 supersession plus ADR-0040 in ADR index/module boundary.

The union retains Settings, Appearance, AppShell, File Manager,
Power/Brightness, virtual desktop, Font, Clipboard, launcher, task list,
customization, Flow workflow, current Team Board/docs/test-harness state, and
all newer manager registry rows. There are zero deleted paths and zero
`ops/team` paths in `d0e0809..82830b9`.

## Exact changed paths (40)

```text
M .github/workflows/ci.yml
A docs/wiki/adr/0030-confine-qtermwidget-behind-terminal-adapter.md
A docs/wiki/adr/0040-own-terminal-child-pty-and-bridge-through-teletype.md
M docs/wiki/adr/index.md
A docs/wiki/apps/terminal.md
M docs/wiki/architecture/module-boundaries.md
M docs/wiki/index.md
M mkdocs.yml
M src/CMakeLists.txt
A src/apps/terminal/CMakeLists.txt
A src/apps/terminal/main.cpp
A src/apps/terminal/org.qindaqt.Terminal.desktop
A src/apps/terminal/session/process_liveness.cpp
A src/apps/terminal/session/process_liveness.h
A src/apps/terminal/session/pty_bridge.cpp
A src/apps/terminal/session/pty_bridge.h
A src/apps/terminal/session/terminal_launch_policy.cpp
A src/apps/terminal/session/terminal_launch_policy.h
A src/apps/terminal/session/terminal_session.cpp
A src/apps/terminal/session/terminal_session.h
A src/apps/terminal/session/terminal_session_backend.cpp
A src/apps/terminal/session/terminal_session_backend.h
A src/apps/terminal/session/terminal_session_types.h
A src/apps/terminal/ui/terminal_appearance.cpp
A src/apps/terminal/ui/terminal_appearance.h
A src/apps/terminal/ui/terminal_widget_adapter.cpp
A src/apps/terminal/ui/terminal_widget_adapter.h
A src/apps/terminal/ui/terminal_window.cpp
A src/apps/terminal/ui/terminal_window.h
M tests/CMakeLists.txt
A tests/apps/terminal/CMakeLists.txt
A tests/apps/terminal/check_cli_rejection.cmake
A tests/apps/terminal/check_desktop_metadata.cmake
A tests/apps/terminal/run_installed_terminal.cmake
A tests/apps/terminal/tst_launch_policy.cpp
A tests/apps/terminal/tst_pty_bridge.cpp
A tests/apps/terminal/tst_terminal_appearance.cpp
A tests/apps/terminal/tst_terminal_session.cpp
A tests/apps/terminal/tst_terminal_widget_adapter.cpp
A tests/apps/terminal/tst_terminal_window.cpp
```

Sorted path SHA-256 is
`43f236f801cdf752a39b8cfd30051f1e292d10f4b95e54aa3108c2119c414e62`;
sorted name-status SHA-256 is
`1ff590439b71469336cd25a122189e09d71ca3474c800259a90806f1601bf028`;
binary manager-base diff SHA-256 is
`645a03324a27b06251c51ef195743270eb6d2d5f8f987d74f3a8c2177e30f9f6`.

## Verification

All generated output is under `/mnt/d/QindaQt/builds`.

- Strict Debug configure: exit 0 with qtermwidget 2.4.x prefix, shell,
  production shell, KWin plugin, and host-uinput rows disabled; strict warnings
  and `CMAKE_AUTOMOC_PATH_PREFIX=ON` enabled.
- Focused build: **63/63** steps, exit 0.
- Clean-environment `ctest -R '^qindaqt\.terminal-'`: **9/9**, exit 0.
- Direct appearance: **7/7**, exit 0.
- Direct production adapter: **4/4**, exit 0.
- `python3 tools/check-source-shape`: exit 0, **1383** files, zero skips;
  production adapter is 496 nonblank lines, below the 500-line review trigger.
- `python3 tools/validate-docs`: exit 0, **93** documents/navigation.
- `/home/cabewse/venv/bin/mkdocs build --strict`: exit 0.
- `git diff --check d0e0809..HEAD`: exit 0.
- `ldd` resolves `libqtermwidget6.so.2` to the exact pinned prefix; underlying
  2.4.0 library SHA-256 is
  `b1440218096965e6161d67fab56d5f4ef6da869ad02cdb8999e98aa95a990dd1`.
- The generated installed-metadata test command names that exact imported
  library, and its disposable stage is removed after success.
- Final porcelain status, uncommitted diff, staged diff, conflict-marker scan,
  and `ops/team` diff are empty.

Dijkstra the 2nd already supplied exact independent source/build PASS and
Church the 3rd supplied exact private-Weston 40/40 PASS for production commit
`a9cc17f`. Because replay production bytes are unchanged, this lane did not
repeat private Weston. The requested reviewer should attack exact `82830b9`
for replay/union correctness, clean package dependency injection, focused
Debug behavior, docs/shape/provenance, and a poison negative that makes the
imported library unavailable; after PASS, the Program Manager can integrate
the exact series and rerun manager-tree gates.
