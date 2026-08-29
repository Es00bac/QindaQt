# Micah Stone Terminal S0 midpoint: implementation complete, static gates pass

- Time: 2026-08-28T13:43:14Z
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0`
- Branch: `worker/terminal-s0`, base `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- State: implementation complete, uncommitted; commit + handoff follow in this
  same lane. Still source/static only — no compiler, no shell/PTY/UI process,
  no host session/display/input.

## Dependency decision (material finding)

`qtermwidget6` 2.4.x (Arch `extra/qtermwidget 2.4.0-1`, LGPL-2.1-or-later) is
the smallest maintainable non-reinvented VT/rendering boundary. I audited the
upstream tag 2.4.0 source (read-only fetch) and pinned the exact teletype
contract in **ADR-0028** (`docs/wiki/adr/0028-confine-qtermwidget-behind-
terminal-adapter.md`, status Proposed): `startTerminalTeletype()` opens an
empty PTY and re-exposes keyboard bytes on the `sendData` signal,
`getPtySlaveFd()` hands the slave to our child, and master reads plus
`TIOCSWINSZ` resize work without a widget-owned child. Because the application
forks/execs its own child, exit codes and signal deaths are real `waitpid`
truth, and process-group teardown is exact and bounded — this is why I chose
the teletype route over letting the widget spawn the shell (which cannot
report exit codes at all).

VTE was rejected (GTK), Konsole internals rejected (no supported library
boundary), and a from-scratch VT parser rejected (reinvention). The dependency
is confined to one adapter translation unit; tests link Qt only.

## Implemented

- `src/apps/terminal/session/`: pure launch policy (argv-only shell
  resolution, hostile env sanitization, forced `TERM`/`COLORTERM`, UTF-8
  locale fallback, view-size clamping), `TerminalSession` state machine with
  bounded master-close → SIGTERM → SIGKILL escalation to the captured process
  group, typed exit publication, restart-as-new-generation, and a
  `ProcessMonitor` seam with the production POSIX implementation (leader
  revalidation guards recycled-PID signaling).
- `src/apps/terminal/ui/`: QST-1 appearance adapter + Konsole-format scheme
  document generator (16 ANSI slots from public token roles, no hex), the
  qtermwidget adapter (only qtermwidget consumer), and the presentation
  window with stable action names and Shift-modified terminal-safe shortcuts
  (no plain Ctrl+C/S/Q/A/Z/X/V/R/K/W bindings: those belong to readline).
- `tests/apps/terminal/`: policy, session lifecycle/teardown, window
  semantics (offscreen), appearance documents, desktop metadata,
  positional-argument CLI rejection, and a staged installed-metadata row.
- Registries: additive lines in `src/CMakeLists.txt`, `tests/CMakeLists.txt`,
  `mkdocs.yml`, wiki index, ADR index, module boundaries (one new row + one
  dependency paragraph), and `qtermwidget` added to both Arch CI pacman lists
  plus the two version-record lines in `.github/workflows/ci.yml`.
- Wiki: owning page `docs/wiki/apps/terminal.md` with honest deferrals.

## Static evidence (exit 0 on this exact tree)

- `python3 tools/check-source-shape` — 1027 files checked
- `python3 tools/validate-docs` — 65 documents
- `uvx --from mkdocs mkdocs build --strict`
- `git diff --check`

This is context, not executable evidence: no test has run. Exact commit and
handoff follow.
