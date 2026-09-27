# Panel feedback candidate handoff

Exact product candidate: `152ab42d028898908ca50e1d2419f40c8767cc21`. Base: `64ae95c3`. Branch: `fix/panel-owner-feedback`.

Changed product paths:
- `src/compositor/kwin/kwinshellwindowactions.{h,cpp}`
- `tests/compositor/ShellWindowActionsTests.cmake`
- `tests/compositor/shellwindowactionsliveprobe.cpp`
- `tests/compositor/shellpanelownerliveproof.{h,cpp}`
- `docs/wiki/architecture/compositor-session.md`
- `docs/wiki/development/testing-harness.md`

Verification:
- `cmake --preset dev`: exit 0; installed Qt QML import warnings remain unrelated.
- `cmake --build build/dev --target qindaqt_shell_window_actions_live_probe qindaqt_shell_window_actions_tests qindaqt_shell_window_identity_tests qindaqt_shell_task_facts_tests -j 6`: exit 0, strict warnings enabled.
- `ctest --test-dir build/dev -R '^compositor\.(shell-window-actions|shell-window-identity|shell-task-facts|kwin-shell-window-actions)$' --output-on-failure --no-tests=error`: exit 0, 4/4 twice, final native 3.45 seconds.
- Mutation with base `kwinshellwindowactions.cpp`: native test exit 8, exact failure `80 repaints and a same-owner panel add/remove produced 84 identity / 84 task invalidations`. Candidate restored and rebuilt before final successful run.
- `python3 tools/docs_validation.py`: exit 0, 416 documents.
- `/home/cabewse/.local/bin/mkdocs build --strict`: exit 0.
- `git diff --check`: exit 0.

The test exercises real raster commits, same-PID panel addition/removal, conflicting second PID admission failure/recovery, final panel destruction/recreation, and authenticated reads. The implementation changes only publication equality; authorization continues fresh role scans. Client teardown revokes before KWin marks tearingDown, and role lifetime disconnects prevent retained commit callbacks.

Caveats: no physical GPU/frame pacing or long-session resource trend is established here. Source inspection does not prove a growing live-role list. The preexisting private nested harness activates private-bus portal services which produce startup warnings; assertions pass and no physical session was touched.

Requested next action: independent exact-commit review, then manager integration and combined deployment gates on both machines. Concrete help offer: repair any blocking reproduction in this worktree or review adjacent notification/lifecycle changes after dispatch. Do not claim this candidate as integrated product progress yet.
