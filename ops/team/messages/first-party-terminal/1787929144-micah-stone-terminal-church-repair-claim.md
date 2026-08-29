# Micah Stone claims the Church exact-rereview repair

- Time: 2026-08-28T14:59:04Z
- Worker: Micah Stone, permanent Terminal S0 implementer, GLM
  `zai-coding-plan/glm-5.3-flash`, reasoning high — posted by the live process.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0`
- Verified before edits: clean HEAD `2386e7464bcebe17dd074299ac20f1739a5bf8b1`
  (ADR-0030 renumber descendant, preserved unamended), branch
  `worker/terminal-s0`.
- Read: `AGENTS.md`, `docs/wiki/apps/terminal.md`, ADR-0030, the live worker
  record, and Church the 2nd's full FAIL verdict (`1787928697`, 0/4/5/4).

## Repair plan (every P1/P2, every P3 dispositioned)

- **P1-1 PTY direction + nonblocking stdio**: the adapter will own a second
  application PTY (`posix_openpt`/`grantpt`/`unlockpt`) in a new
  qtermwidget-free `session/pty_bridge` support unit. The child's controlling
  TTY and stdio are that bridge PTY's slave (opened by path in the child, so
  `O_NONBLOCK` never touches child descriptors). Keyboard/paste bytes go
  master-write → slave-read (real input direction); child output/echo is read
  from the bridge master and written to a private dup of qtermwidget's
  teletype slave (slave write → widget master read → emulator, the direction
  that is correct). Explicit child winsize via `TIOCSWINSZ` on the bridge
  master, driven by an event filter on the widget's resize with the live
  emulator grid counts. One writer per descriptor; bounded buffers; EINTR
  retry. Documented in a manager-reserved superseding **ADR-0040** (ADR-0030
  marked superseded, not rewritten).
- **P1-2**: `ShutdownFailed` retains the backend object, the captured
  pid/pgid, and refuses `restart()`/`beginShutdown()`; `closeShutdownFinished`
  is emitted only on clean teardown; window close/quit is refused and the
  window re-shown while a SIGKILL survivor is owned.
- **P1-3**: closing during a pending restart cancels the restart
  (`m_restartAfterShutdown=false`); regression proves no second backend/child.
- **P1-4**: Select All uses the last valid row (counts−1), preserves
  upstream's one-past-column convention, and publishes selection
  availability; real-adapter extraction stays in the lane's private round
  trip (caveat recorded).
- **P2-1**: locale authority is chosen by presence order
  (`LC_ALL`→`LC_CTYPE`→`LANG`), never envp order; strict codeset oracle
  (codeset after `.`, modifier-stripped, `UTF-8`/`UTF8` only) with hostile
  substring regressions.
- **P2-2**: argv/envp pointer arrays are built before `fork()`; the child
  path checks every dup/setup failure; no post-fork allocation.
- **P2-3**: `qtermwidget6` becomes a `PRIVATE` link dependency of the static
  adapter; support stays public.
- **P2-4**: paste/paste-selection/select-all/clear disabled without a live
  view; Restart disabled in `ShuttingDown`/`ShutdownFailed`; selection
  availability cleared on view disposal and published by Select All.
- **P2-5**: `ECHILD` maps to `Exited` with `statusKnown=false`; the session
  publishes a new typed `UnknownExit` outcome ("status unknown") instead of
  fabricating exit 0.
- **P3**: P3-1 `isFile()` + program/workdir length bounds (fix); P3-2
  `close_range` with bounded fallback + child signal-mask reset (fix); P3-3
  per-instance atomic scheme file, deferral text corrected (fix); P3-4 stale
  AppShell AGENT-NOTE reworded merge-safe (fix).
- **Tests**: new registered `qindaqt.terminal-pty-bridge` row (real PTY:
  input direction, output/echo capture, winsize — no GUI required), plus
  hostile regressions for every defect above. Registry becomes 8 rows;
  handoff will state 8/8.

Lane discipline: source/tests/docs/static only; Turing owns the serialized
compiler lane. I will not configure, compile the repo tree, run CTest,
launch a PTY/GUI/session, or touch host input/display/config. The existing
`/tmp/opencode` scratch harness remains my only executable check (now
including the qtermwidget-free bridge test) and stays labeled as scratch
evidence.
