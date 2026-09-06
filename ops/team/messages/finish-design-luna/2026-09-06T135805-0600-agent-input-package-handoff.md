# Luna — installed agent-input helper handoff

- Time: `2026-09-06T13:58:05-06:00`
- Exact base: `85a249d8`
- Candidate implementation commit: `0f3625b438656d7cbf20b89c7bb73cf8b0d95793`
- Candidate worktree: `/home/cabewse/work_SPaC3/container-wm/.cache/finish-agent-input-package`
- Requested next action: independent exact review, then manager integration.

## Outcome

The approved RemoteDesktop portal helper is now installable as the normal
`qindaqt-agent-input` terminal command. The `AgentInput` install component
places the executable and its adjacent `agent_input` Python package under the
configured `${CMAKE_INSTALL_BINDIR}`. The installed command therefore imports
its package from the installation tree and has no checkout or symlink fallback.
The existing client logic and portal approval lifecycle are unchanged. The
runtime dependency documentation names Python 3.10+, `dbus-python`,
PyGObject (`gi.repository.GLib`), a session D-Bus, and a portal backend that
implements `org.freedesktop.portal.RemoteDesktop`.

## Changed paths

- `CMakeLists.txt`
- `docs/wiki/adr/0087-agent-input-via-remotedesktop-portal.md`
- `tests/tools/CMakeLists.txt`
- `tests/tools/run_installed_agent_input_smoke.cmake`

## Acceptance evidence

All commands were run in the assigned worktree and used at most two build
workers. No live input, host portal approval, or root privileges were used.

- Private Debug configure with `BUILD_TESTING=ON`, shell/plugin production
  disabled, and strict warnings: exit 0.
- Installed smoke gate:
  `ctest --test-dir /home/cabewse/work_SPaC3/container-wm/.cache/agent-input-build -R '^qindaqt\\.agent-input-installed$' --output-on-failure`
  exit 0, 1/1 passed. The gate stages the `AgentInput` component under a
  temporary prefix, verifies the executable/package files, clears
  `PYTHONPATH`, and runs installed `qindaqt-agent-input --help`; it does not
  open a bus or request portal approval.
- Existing Python tools suite:
  `PYTHONPATH=tools python3 -m unittest discover -s tests/tools -p 'test_*.py'`
  exit 0, 28/28 passed.
- `./tools/validate-docs`: exit 0, 182 Markdown documents validated.
- `/home/cabewse/work_SPaC3/container-wm/.cache/handbook-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/container-wm/.cache/agent-input-mkdocs`: exit 0.
- `git diff --check`: exit 0.

The package smoke is deliberately limited to import/help behavior. Actual
portal approval, device selection, event dispatch, and revocation remain
covered by the existing private fake-portal lifecycle tests and require the
normal session portal at runtime.
