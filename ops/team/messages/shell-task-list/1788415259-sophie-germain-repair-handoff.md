# Sophie Germain — compositor shell window actions repair handoff

- Handoff time: 2026-09-03T00:00:59-06:00
- Exact candidate commit: `3690a056e667135d486da4fa60d7996882a4560a`
- Candidate tree: `ae70ed0f56ac200baa74bb68341916dc4bc2322a`
- Exact repair base/parent: `ffa6cfef1b71f9c52f7431d6daaa7beb132d6b7a`
- Rejected product ancestor repaired: `11f4c0a85851376623c34bfb8cfda2ddb5383bb3`
- Original lane base: `d0b70ed80d9c6bf45b9d3b6219d1e11514514c4c`

## Changed product paths

- `docs/wiki/adr/0057-authenticate-shell-window-actions-by-panel-owner.md`
- `docs/wiki/architecture/compositor-session.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/compositor-control-v1.md`
- `src/compositor/include/qindaqt/compositor/shellwindowactions.h`
- `src/compositor/kwin/kwinshellwindowactions.cpp`
- `src/compositor/src/shellwindowactions.cpp`
- `tests/compositor/shellwindowactionsliveprobe.cpp`
- `tests/compositor/tst_shellwindowactions.cpp`
- `tests/shell_window_actions_client/tst_shellwindowactionsprivatebus.cpp`

## Outcome

The D-Bus endpoint now forwards untouched wire strings into the controller. The controller reads only O(1) length metadata before authenticating the live panel-owner/caller PID join; only an authenticated, rate-admitted, 64/128/20-bounded request reaches revision, generation, or UUID parsing. Unbound, unauthenticated, rate, and oversized failures are fixed compact responses with no caller-field echo. Unit and nested tests use megabyte-scale hostile fields and prove no registry lookup or reflection. The real private-bus client row now replaces the well-known owner while the old exact owner holds a delayed reply, proves the pending result becomes uncertain with no replay to the replacement, rejects the late old-owner reply, and separately proves request timeout on the real transport.

## Verification evidence

All commands ran from the lane worktree and exited `0`.

- Exact required Debug and Release configure commands from the lane brief completed with `QINDAQT_ENABLE_STRICT_WARNINGS=ON`, KWin plugin and production shell enabled, testing enabled, and host uinput disabled. CMake retained the repository's existing mixed-prefix runtime-path warnings.
- `cmake --build <ROOT>/{debug,release} --parallel 3 --target qindaqt_compositor qindaqt_shell_window_actions_tests qindaqt_shell_window_actions_live_probe qindaqt_shell_window_actions_client_tests qindaqt_shell_window_actions_private_bus_tests tests/compositor/all` completed in both profiles.
- `ctest --test-dir <ROOT>/debug -R '^compositor\.' --output-on-failure --no-tests=error`: 48/48 passed; the serial private `compositor.kwin-shell-window-actions` row passed with the new hostile-size stage.
- `ctest --test-dir <ROOT>/release -R '^compositor\.' --output-on-failure --no-tests=error`: 48/48 passed; the same nested row passed.
- `ctest --test-dir <ROOT>/{debug,release} -R '^qindaqt\.shell-window-actions-(client|private-bus)$' --output-on-failure --no-tests=error`: 2/2 passed in each profile.
- Development iterations before the final dual-profile gate: focused Debug action/client/private-bus 3/3, nested KWin 1/1, then the tightened server/private-bus controls 2/2; all exited `0`.
- `./tools/validate-docs`: validated 119 Markdown documents and navigation.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`: passed.
- `./tools/check-source-shape`: checked 1,829 files; only the pre-existing 500-line compositor CMake and unrelated 539-line display-color test decomposition warnings were reported.
- `git diff --check`: passed.
- `PYTHONPYCACHEPREFIX=<ROOT>/pycache python3 -m py_compile tests/compositor/test_shell_window_actions_nested.py tests/compositor/test_dbus_contract.py`: passed.
- JSON gate: zero JSON files changed, so `python3 -m json.tool` was not applicable.

## Bounded caveats

- This repair claims private-bus, deterministic unit, and virtual KWin 6.6.5 evidence only. It does not claim host desktop/session-bus, `tests/session`, hardware, uinput, physical DRM/GPU, or live Hybrid-group coverage.
- The accepted ADR's documented residual PID-reuse/role-impersonation threat model is unchanged.
- The reviewer-only scratch driver was not modified; the product nested row now contains its wrong-PID hostile-size control.

## Requested next action

Margaret Rock (OpenAI Codex) should independently recheck exact candidate `3690a056e667135d486da4fa60d7996882a4560a`, including both prior P1/P2 reproductions. If accepted, the Program Manager should integrate that exact candidate.
