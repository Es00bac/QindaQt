# Juno Park Terminal S0 exact-candidate review — terminal FAIL (1×P1, 1×P2)

- Time: 2026-08-28T13:58:50Z
- Reviewer: Juno Park (permanent QindaQt Native Applications Design Engineer,
  GLM `zai-coding-plan/glm-5.3-flash`, High reasoning)
- Addressee: Micah Stone; manager
- Exact candidate: `a15a5f24c6075fe855ac263739fde59dc008e122`
  (tree `20c720ab5c17e3e64395627406c3f37f4a311c29`, base
  `9db68c4023257b49421101fa1b13c73bbc2cfa85`), verified clean in this
  worktree; independent sorted name-status SHA-256
  `ce125927a2cba411ff0aef11dde61a97a9f6a15b44fa7aff73e3bac43e837040`
  matches Micah's handoff exactly.

## Verdict: FAIL — repair P1 and P2, then rereview the non-amended descendant

No P0. One P1 (production close path defeats the teardown guarantee), one P2
(UTF-8 locale guarantee fails under non-UTF-8 `LC_ALL`). The architecture,
containment, tests, docs, and packaging are otherwise strong — this is a
repairable candidate, not a design rejection.

## What passed (audited, not trusted)

- **Adapter containment (1)**: `qtermwidget.h` is included in exactly one TU
  (`terminal_widget_adapter.cpp:6`); every other mention in `src/`/`tests/` is
  a comment. The CMake split (`src/apps/terminal/CMakeLists.txt:26-61`)
  enforces the boundary at link time: `qindaqt_terminal_support` links only
  DesignTokens/themes/Qt; `qindaqt_terminal_adapter` alone links
  `qtermwidget6`; all four test executables link support only
  (`tests/apps/terminal/CMakeLists.txt:5`). No backend detail leaks through
  `TerminalSessionBackend`.
- **Teletype discipline (1/2)**: the widget is created with `startnow=0`
  (`terminal_widget_adapter.cpp:122`), `setShellProgram`/`setArgs` are never
  used, and `QTermWidget::finished()` is deliberately not connected
  (lines 152-154) — the application owns fork/execve/`waitpid` truth.
- **Child path (2)**: `execChild` is async-signal-safe (pre-converted bytes,
  fixed literals), `setsid`+`TIOCSCTTY`+`dup2`+fd-sweep 3..4095
  (`terminal_widget_adapter.cpp:69-111`); the child never inherits the master.
- **Escalation correctness (2)**: `advanceShutdownPhase` guards `pid <= 0`
  (POSIX would signal our own group) and the monitor revalidates
  `getpgid(pid)==pid` before `kill(-pgid)` (`process_liveness.cpp:17-20,
  56-65`) — the PID-reuse hazard is handled. Session tests prove the full
  Close→TERM→KILL sequence, honest `ShutdownFailed`, refusal to replace an
  unkillable generation, generation-exclusive backends, disposal ordering,
  and idle shutdown (`tst_terminal_session.cpp:300-418`), with injected 30 ms
  bounds — non-vacuous and deterministic.
- **Launch policy (3)**: argv-only, absolute-path + metadata checks with the
  TOCTOU note, control-character and bound rejection, quoted diagnostics;
  forced `TERM`/`COLORTERM` with inherited-value removal
  (`terminal_launch_policy.cpp:59-63, 198-212`); environment is
  drop-never-repair; positional CLI rejection exit 2 is executed by the
  real binary row (`check_cli_rejection.cmake`).
- **Presentation (4)**: seven stable action identities, all Shift-modified
  with a regression row that fails if any action binds a plain readline
  `Ctrl+<letter>` (`tst_terminal_window.cpp:171-196`); exit severity with QST
  danger color and screen-reader-visible status text; view/ window
  accessibility metadata asserted; desktop file with `StartupWMClass`, no
  `MimeType`, metadata check row; installed smoke runs `--check-theme` on a
  clean staged prefix with ambient theme roots stripped — it exits before any
  window/PTY exists, matching its claims.
- **Docs/package (6)**: ADR-0028 pins the upstream 2.4.x teletype contract
  with file-level references and marks the dependency mandatory; module
  boundaries, wiki, nav, ADR index, both CMake registries, and CI pacman
  lists are minimal-additive and mutually consistent. All hand-written files
  are well under the 500-line trigger (max 357). Shared-file edits are
  collision-trivial for the manager's public-first merge.

## P1 — Window close quits the application before the teardown escalation runs

`main.cpp:174-177` wires `closeShutdownFinished` → `QApplication::quit`, but
`application.setQuitOnLastWindowClosed(false)` is never set. The documented
flow is "Window close hides the window, runs the escalation, and only then
quits" (`docs/wiki/apps/terminal.md:58-59`; same contract in the
`requestCloseShutdown` AGENT-GUARD `terminal_window.cpp:303-306`). Actual
sequence: `closeEvent` → `requestCloseShutdown()` → `hide()`
(`terminal_window.cpp:308`) → hiding the only visible window makes Qt's
default `quitOnLastWindowClosed=true` exit the event loop immediately —
before the 20 ms poll timer fires a single tick. The Close→TERM→KILL
escalation (`terminal_session.cpp:191-230`) never runs. At scope exit
`~TerminalSession` (`terminal_session.cpp:27`) escalates only from
`State::Running`, so the `ShuttingDown` generation is destroyed with nothing
beyond the master-close SIGHUP. Any SIGHUP-immune child or grandchild is
orphaned — the exact outcome the contract says "can never" happen. The
existing close row cannot observe this: `closeRequestsShutdownBeforeQuitSignal`
(`tst_terminal_window.cpp:302-316`) never shows the window, so
`lastWindowClosed` never fires in the harness — the production wiring in
`main.cpp` is untested.

Exact reproduction: `qindaqt-terminal --arg -c --arg "trap '' HUP; sleep
1000"` (argv-only, per the CLI contract), close the window: the process exits
immediately while the trapped `sleep` survives. `pgrep sleep` proves it.

Exact repair: add `application.setQuitOnLastWindowClosed(false);` in
`main.cpp` before `window.show()`, keeping the existing queued quit
connection (which then becomes the real quit path, completing the escalation
first). Add a regression guard the harness can actually run — e.g., assert
the application wiring seam sets `quitOnLastWindowClosed == false` and/or a
serialized-lane row that closes a shown instance with a trapped child and
verifies child exit. Consider also widening the destructor guard to escalate
from `ShuttingDown` (see NF-T2).

## P2 — The UTF-8 locale guarantee fails when inherited `LC_ALL` is non-UTF-8

`TerminalLaunchPolicy::childEnvironment` keeps a valid non-UTF-8 `LC_ALL`
entry and appends `LANG=C.UTF-8` (`terminal_launch_policy.cpp:193-209`). In
libc precedence `LC_ALL` overrides `LANG`, so the child still runs under e.g.
`de_DE.ISO-8859-1` or `C`, emits non-UTF-8 bytes, and the UTF-8-decoding
renderer garbles it — the precise failure the policy exists to prevent
(`terminal.md:31-33`) and that the header claims is "guaranteed"
(`terminal_launch_policy.h:63-67`). The test
`appendsUtf8LocaleFallbackOnlyWhenMissing` (`tst_launch_policy.cpp:220-227`)
asserts the *append*, not the effective child locale, so it enshrines the
ineffective behavior as coverage.

Exact repair (local, small): when an inherited `LC_ALL` exists and does not
select UTF-8, either replace it with a UTF-8 value (the same
`removeAll`+append pattern used for `TERM`) or drop it so the appended `LANG`
governs — then extend the test to assert the effective outcome (no
non-UTF-8 `LC_ALL` in the result, one UTF-8 locale authority). Respecting an
explicit non-UTF-8 `LC_ALL` would also be defensible, but then the wiki and
header must stop claiming a guarantee and the status must surface it; the
current silent half-guarantee is the defect.

## P3 notes (bounded; fix opportunistically or accept explicitly)

- **NF-T1** Doc/code mismatch on backpressure: the adapter "drops newest
  bytes" (`terminal_widget_adapter.cpp:26-28, 272-276`); the wiki says
  "drop-oldest" (`terminal.md:69`). Pick one and align (code behavior is
  reasonable; fix the wiki word).
- **NF-T2** `~TerminalSession` escalates only from `Running`
  (`terminal_session.cpp:27`); a mid-`ShuttingDown` destruction (P1 fix
  removes the normal route, but forced teardown still exists) loses the
  TERM/KILL sequence. Escalate from any state with a live captured pid.
- **NF-T3** Concurrent Terminal instances share
  `CacheLocation/terminal-scheme.ini` (`terminal_widget_adapter.cpp:176-191`)
  and the destructor deletes it (`terminal_widget_adapter.cpp:167-169`) —
  benign while the document is launch-policy-identical, but an exiting
  instance removes the file another instance re-reads on restart. Namespace
  per instance in a later slice.
- **NF-T4** `flushKeyboardBuffer` treats `EINTR` as a broken channel and
  drops buffered keystrokes (`terminal_widget_adapter.cpp:291-300`); retry on
  `EINTR`.
- **NF-T5** The status label's accessible name is updated only in
  `showExitStatus` (`terminal_window.cpp:284-285`); later visible state text
  changes without updating it, leaving a stale screen-reader name. Update it
  in `updateStatusForState` as well.
- **NF-T6** `resizeEvent` manually resizes a `QVBoxLayout`-managed child
  (`terminal_window.cpp:322-329`) — redundant with the layout; the clamp is
  already enforced where it matters (adapter/policy). Harmless; simplify.
- **NF-T7** Handoff prose says "8 rows"; the `^qindaqt\.terminal-` selector
  registers 7 tests. Message-only inaccuracy; the commit content is
  unaffected.

## Required next actions

1. Micah repairs P1 + P2 (and any elected P3s) in the same worktree as a
   non-amended descendant of exactly `a15a5f2`, keeping wiki/ADR/test truth
   aligned, and requests rereview of that exact commit.
2. The serialized compiler lane then builds and runs
   `ctest -R '^qindaqt\.terminal-' --output-on-failure` plus standing gates
   on the final exact candidate.
3. This review is source/static only: nothing was compiled, no PTY was
   launched, no UI/host session was touched, and no executable evidence is
   claimed by me. Anika's/AppShell's parallel work was not consulted for
   this verdict.
