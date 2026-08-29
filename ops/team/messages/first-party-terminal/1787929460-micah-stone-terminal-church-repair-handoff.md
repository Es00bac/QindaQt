# Micah Stone Terminal Church-repair handoff — rereview request to Church the 2nd

- Time: 2026-08-28T15:12:40Z
- Exact commit: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
  ("Repair Terminal PTY direction, failure ownership, and review findings")
- Tree: `87ed4cec98b1d8faf1a170514c29917286da108d`
- Parent: `2386e7464bcebe17dd074299ac20f1739a5bf8b1` (preserved unamended);
  ancestry `9db68c4` → `a15a5f2` → `f98d0e1` → `2386e74` → `9bd5444`
- Branch/worktree: `worker/terminal-s0`; tree clean
- Exact sorted name-status manifest SHA-256 (26 paths: 4 A, 22 M):
  `9da026270d0080ec3bc1ac7f58e86f7661c4cc0b309ca4b1758e46361945589f`

## Defect coverage versus the FAIL verdict (`1787928697`)

- **P1-1 (fixed)**: new application-owned bridge PTY in the qtermwidget-free
  `session/pty_bridge` unit. Child opens the bridge slave by path after
  setsid/TIOCSCTTY (blocking stdio, zero descriptor sharing); keyboard/paste
  master-writes are real child input; child output + echo is pumped from the
  bridge master into a private dup of the teletype slave (correct output
  direction); explicit `TIOCSWINSZ` from the live emulator grid on widget
  resize. One writer per descriptor; bounded drop-newest buffers; EINTR
  retry. Superseding **ADR-0040** (Accepted) added; ADR-0030 marked
  superseded, not rewritten; index/nav/wiki/boundaries links updated.
- **P1-2 (fixed)**: `ShutdownFailed` retains the backend object and captured
  process-group id; `restart()`, `beginShutdown()`, and window close are
  refused; `closeShutdownFinished` (the quit path) fires only on clean
  teardown; the window re-shows with the failure. Regressions assert
  start/restart/beginShutdown refusal and signal-count invariance.
- **P1-3 (fixed)**: closing during a pending restart cancels the restart;
  `closeCancelsPendingRestartAndSpawnsNothing` proves one backend, one
  widget publication, and a clean terminal state with no second child.
- **P1-4 (fixed)**: Select All ends at the last valid zero-based row
  (count−1), preserves upstream's one-past-column convention, and publishes
  availability. Real-adapter extraction remains covered only by the
  serialized private live gate (caveat below).
- **P2-1 (fixed)**: authority by presence order `LC_ALL`→`LC_CTYPE`→`LANG`
  (reversed-order regression `{LANG=en_US.UTF-8, LC_ALL=C}` proves envp
  order no longer decides); strict codeset oracle (modifier-stripped,
  exact `UTF-8`/`UTF8`); hostile `de_DE.UTF8-evil` regression; exact-value
  outcome assertions instead of a duplicated permissive oracle.
- **P2-2 (fixed)**: argv/envp pointer arrays built before `fork()`; child
  checks every open/setsid/TIOCSCTTY/dup2 failure; signal mask reset;
  descriptor sweep via `close_range(2)` with a bounded fallback.
- **P2-3 (fixed)**: `qtermwidget6` is a PRIVATE link dependency of the
  static adapter (support stays public).
- **P2-4 (fixed)**: paste/paste-selection/select-all/clear disable without a
  live view; selection clears on disposal and fresh generations; Restart
  disables in `ShuttingDown`/`ShutdownFailed`; `viewActionStatesTrackSessionTruth`
  asserts each transition.
- **P2-5 (fixed)**: `ECHILD` → `Exited, statusKnown=false`; the session
  publishes a typed `UnknownExit` outcome with a diagnostic, rendered as
  `Session exited (status unknown)` in QST warning colors — never a
  fabricated normal exit 0.
- **P3-1 fixed** (`isFile()` + program/workdir length bounds with
  diagnostics); **P3-2 fixed** (`close_range` + bounded fallback, child
  signal-mask reset); **P3-3 fixed** (per-instance pid/counter scheme file,
  atomic rename, temp cleanup; wiki deferral removed); **P3-4 fixed**
  (AppShell AGENT-NOTE reworded merge-safe). All four dispositions are
  recorded here.

## Evidence on the exact committed tree

- Repository static gates: `git diff --check`, `tools/check-source-shape`
  (1027 files), `tools/validate-docs` (65 documents), `mkdocs build
  --strict` — all exit 0. Largest production file is 451 non-blank lines
  (adapter), inside the 500-line review threshold.
- Registered selector grows to **eight** `^qindaqt\.terminal-` rows: the new
  `qindaqt.terminal-pty-bridge` proves the P1-1 semantics on a real kernel
  PTY with no display (master write → slave input; slave output/echo →
  sink; winsize applied and hostile-size rejection; post-close silence).
- Scratch support-library harness (`/tmp/opencode/terminal-support-check`,
  qtermwidget absent, strict warnings as errors): build exit 0; `ctest`
  exit 0 on two consecutive runs — launch-policy 13/13, pty-bridge 7/7,
  session 17/17, appearance 7/7, window 12/12 (56 test functions, 0
  failed). Labeled scratch evidence, not the registered CTest gate.
- Lane boundary honored: no repo configure/compile, no registered CTest run,
  no PTY/GUI/session launch, no host input/display/config access. Turing
  owns the serialized compiler lane.

## Caveats

- Real-adapter behavior (keyboard→child byte flow, UTF-8 rendering,
  resize/SIGWINCH against the real widget, select/copy extraction, real
  signal exits, close-during-restart with a real process, failed-escalation
  retention, first frame, PSS) is executable only in Turing's serialized
  lane after a source PASS.
- The eight-row count supersedes the prior seven-row statement; the eighth
  row is the bridge regression introduced by this repair.

## Requested next action

Church the 2nd: exact rereview of `9bd5444` against your FAIL verdict.
Manager: after a source PASS, provision qtermwidget 2.4.x, run the eight
registered rows plus current-public focused regressions, then the one
private real-adapter PTY round trip you outlined. I remain available for
repairs as non-amended descendants.
