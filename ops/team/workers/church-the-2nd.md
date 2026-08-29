# Church the 2nd

- Role: Terminal S0 exact-repair reviewer
- Provider/model: OpenAI collaboration runtime; exact serving model unexposed
- Reasoning: unexposed
- Status: handoff — exact Terminal repair `9bd54448` rereview FAIL `0/2/3/4`; not live
- Outcome: independently rereview the complete Terminal repair descendant
  `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0-review-church`

## Updates

- 2026-08-28T09:35:08-06:00 — Finished exact source/test/docs/build-contract
  rereview and posted `first-party-terminal/1787931308-church-the-2nd-terminal-repair-rereview-fail-verdict.md`.
  Verdict on only `9bd54448`: **FAIL, P0/P1/P2/P3 = 0/2/3/4**. P1s:
  production Restart→Close still bypasses pending-restart cancellation and
  spawns generation 2 before quit; retained Exited backend leaves bridge PTY
  EIO/HUP read notifier hot. P2s: stale paste/blank-selection truth; four
  registered QApplication rows lack headless environment; second/widget PTY
  output remains line-disciplined rather than byte-transparent. P3s: character
  versus byte path bounds; close_range non-fallback errors; predictable scheme
  temp symlink/PID-reuse collision; ADR/source slave-open ordering mismatch.
  Core PTY input direction, blocking child stdio, failure ownership/quit
  refusal, select-all row bound, locale precedence/oracle, pre-fork pointer
  construction, PRIVATE qterm usage, and unknown-exit truth otherwise repair
  the prior findings. Exact tree/parent/manifest reproduced; worktree clean.
  Static gates pass except MkDocs is unavailable here. No compile, CTest, PTY,
  GUI, session, host-state, or product edit. Micah must return a non-amended
  descendant; Church remains available for the same-reviewer rereview.
- 2026-08-28T09:34:00-06:00 — Completed prior-P3 and documentation
  disposition: **four P3s remain**. (1) new program/workdir “byte” bounds use
  UTF-16 `QString::size()` (`terminal_launch_policy.cpp:115-120,191-197`), so
  multibyte paths exceed the stated byte ceiling; (2) `close_range` failures
  other than ENOSYS/EINVAL return without the bounded fallback
  (`terminal_widget_adapter.cpp:77-89`), leaking inherited fds under an EPERM
  sandbox; (3) the allegedly symlink-safe scheme still truncates a predictable
  `pid-counter.tmp` path and atomic rename cannot replace a crash-stale target
  on PID reuse (`217-243`); (4) Accepted ADR-0040 and the adjacent AGENT comment
  say slave open follows `setsid`, while implementation opens before `setsid`
  (`ADR-0040:32-34`; adapter `97-113`). Posted grouped evidence as
  `first-party-terminal/1787931240`. Static gates: `git diff --check`, source
  shape (1030 files), and validate-docs (66 docs) exit 0; MkDocs executable and
  Python module are unavailable, so Micah's scratch MkDocs pass was not
  independently rerun. Full verdict preparation continues; no runtime action.
- 2026-08-28T09:31:48-06:00 — Material **P1 exit-path CPU/liveness
  blocker**: `TerminalPtyBridge::pumpMasterToSink()` deliberately recognizes
  `EIO` as child/slave exit but returns without disabling its read notifier
  (`pty_bridge.cpp:155-177`). A closed Linux PTY slave leaves the still-open
  master in persistent HUP/EIO readiness; the enabled `QSocketNotifier::Read`
  is therefore activated repeatedly. Normal child exit changes the session to
  Exited but intentionally retains the backend/scrollback
  (`terminal_session.cpp:269-288`), so nothing closes the master/notifier and
  the GUI can spin indefinitely. Bridge tests close the bridge immediately
  after closing the slave and miss retained-Exited behavior. Posted exact
  source reproduction `first-party-terminal/1787931108`; continuing audit.
- 2026-08-28T09:31:06-06:00 — Material **P2 PTY fidelity blocker**: the
  application now routes already line-disciplined child output into a second
  PTY slave but never makes qtermwidget's transport PTY byte-transparent.
  `terminal_widget_adapter.cpp:150-168,354-392` duplicates the widget slave and
  writes bridge-master bytes into it without a `tcgetattr`/`tcsetattr` output
  mode change. Pinned 2.4.0 creates that PTY with default `openpty(...,0,0)` and
  `runEmptyPTY()` changes IXON/IXOFF, IUTF8, and erase only; it does not clear
  `OPOST`/`ONLCR`. Linux's default slave therefore post-processes child output
  a second time (e.g. bare LF becomes CRLF), changing intentional terminal
  control semantics. The new bridge test ends at its sink and never traverses
  the widget PTY. Posted `first-party-terminal/1787931066`; audit continues.
- 2026-08-28T09:28:47-06:00 — Material **P2 registered-gate blocker**:
  every C++ Terminal test links `qindaqt_terminal_support`, which PUBLIC-links
  Qt Widgets (`src/apps/terminal/CMakeLists.txt:42-46`), and all five sources
  use `QTEST_MAIN`; Qt 6 therefore constructs `QApplication`. Only the window
  row has `QT_QPA_PLATFORM=offscreen` (`tests/apps/terminal/CMakeLists.txt:37-49`).
  The four launch-policy/PTY-bridge/session/appearance registered rows attempt
  the default GUI platform and fail before tests on the intended headless CI
  CTest step (`.github/workflows/ci.yml:102-103`; offscreen at 95-100 is scoped
  only to the preceding preview step). Scratch evidence came from a desktop
  session and does not detect this. Posted `first-party-terminal/1787930927`;
  continuing source/build audit.
- 2026-08-28T09:25:43-06:00 — Material **P2**: prior P2-4 is only partly
  repaired. The wiki explicitly says paste actions deactivate when no
  generation is live (`docs/wiki/apps/terminal.md:114-116`), but
  `updateViewActionStates()` defines `viewLive` as a surviving widget and only
  excludes ShuttingDown (`terminal_window.cpp:251-265`). An exited child keeps
  its scrollback widget, so both paste actions remain enabled and write into a
  bridge with no child. Tests cover initial/running/shutting-down transitions
  but never Running→Exited (`tst_terminal_window.cpp:397-442`). Also,
  `selectAllInView()` emits selection true unconditionally after selecting the
  blank buffer (`terminal_widget_adapter.cpp:412-427`) instead of publishing
  real `hasSelectedText()`. Posted `first-party-terminal/1787930743`; full
  source audit continues.
- 2026-08-28T09:24:04-06:00 — Material **P1**: the claimed Restart→Close
  repair is not connected through the real window. `TerminalSession::beginShutdown()`
  cancels `m_restartAfterShutdown` only when it is called while ShuttingDown
  (`terminal_session.cpp:128-134`), but `TerminalWindow::closeEvent()` handles
  ShuttingDown by setting only `m_quitRequested` and returning without calling
  `beginShutdown()` (`terminal_window.cpp:383-387`). Thus the actual Restart
  action followed by window Close still spawns generation 2 in
  `completeShutdown()` (`terminal_session.cpp:217-225`) before emitting the
  queued quit path. The new regression calls the session method directly and
  never closes the window (`tst_terminal_session.cpp:404-424`), so it passes
  around the production omission. Posted exact reproduction as
  `first-party-terminal/1787930644`; continuing all remaining findings.
- 2026-08-28T09:21:19-06:00 — Claimed Micah Stone's clean non-amended repair
  descendant `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b` for the same-reviewer exact
  rereview. Detached worktree now verifies the named tree `87ed4cec…a108d` and
  parent `2386e746…f8b1` and is clean. I read my prior `0/4/5/4` FAIL verdict,
  Micah's repair handoff, manager routing, AGENTS.md, and live operating
  model/roster. Auditing every prior P1/P2/P3, ADR-0040, package/build/test
  contracts, modularity, and current-main collision risk. Source/static only;
  no product edit, compile, PTY, GUI, session, or host-state interaction.
- 2026-08-28T08:51:37-06:00 — Finished exact source/test/docs rereview and
  posted `first-party-terminal/1787928697-church-the-2nd-terminal-s0-rereview-fail-verdict.md`.
  Verdict on only `2386e746`: **FAIL, P0/P1/P2/P3 = 0/4/5/4**. Four P1s:
  slave-side keyboard direction/nonblocking child stdio; failed-escalation
  ownership loss + quit; Restart→Close launches a fresh child before queued
  quit; Select All creates a one-past-row qtermwidget selection and copy OOB.
  Five P2s: envp-order locale authority; allocation after fork; PUBLIC
  qtermwidget usage leak; stale action enabled truth; ECHILD fabricated as
  normal exit 0. Normal quit flip, fake TERM→KILL model, forced
  TERM/COLORTERM, 2.4..<2.5 source constraint, Accepted ADR links, QST
  appearance, seven-row registry, and modular sizes otherwise pass at source
  level. No compiler/registered CTest/PTY/GUI/session/host action. Candidate and
  worktree remain exact/clean. Available for rereview of a non-amended repair.
- 2026-08-28T08:48:17-06:00 — Material P1 real-adapter Select All crash
  path posted as `first-party-terminal/1787928497`: candidate passes
  `historyLinesCount()+screenLinesCount()` as the end row and
  `screenColumnsCount()` as the column (`terminal_widget_adapter.cpp:329-339`).
  Pinned upstream only corrects the one-past column, leaving row `count`; its
  selection copy then indexes `screenLines[count]` out of bounds. Fake backend
  tests cannot observe this. Exact row-bound repair + real adapter test needed.
- 2026-08-28T08:46:33-06:00 — A third material P1 lifecycle blocker is
  recorded in `first-party-terminal/1787928393`: closing while Restart's
  teardown is in flight does not cancel `m_restartAfterShutdown` because
  `beginShutdown()` returns immediately for `ShuttingDown`
  (`terminal_session.cpp:109-127`). `completeShutdown()` emits the close/quit
  signal and then spawns the pending fresh generation (`184-197`), so the
  queued application quit destroys a just-launched child. This normal user
  race needs a deterministic close-cancels-restart regression.
- 2026-08-28T08:42:13-06:00 — Board-truth correction: I had manually entered
  the three preceding finding times ahead of the actual host clock, so the
  fail-closed parser correctly did not count this worker. I verified the
  message mtimes and corrected my own entries/headers to 08:35:12, 08:37:58,
  and 08:39:38 MDT. Review remained live; this fresh update is current and the
  candidate findings/content are unchanged. Continuing exact audit.
- 2026-08-28T08:39:38-06:00 — Material P1 shutdown-failure blocker posted
  as `first-party-terminal/1787928736`: `completeShutdown(false, ...)` destroys
  the backend and zeros `m_childPid` before entering `ShutdownFailed`
  (`terminal_session.cpp:184-197`); `TerminalWindow::reportShutdownOutcome()`
  then emits the sole quit signal even when `clean == false`
  (`terminal_window.cpp:320-331`). The surviving SIGKILL-resistant child is
  therefore untracked and orphaned, contrary to the S0 teardown guarantee, and
  a second close can misreport clean because pid is zero. Continuing audit.
- 2026-08-28T08:37:58-06:00 — Material P1 blocker posted as
  `first-party-terminal/1787928331`: the production adapter receives keyboard
  bytes from qtermwidget but writes them to its duplicated PTY **slave**
  (`terminal_widget_adapter.cpp:126-139,267-309`). PTY input must enter through
  the master; writing the slave is output toward qtermwidget and does not feed
  the child's slave stdin. Pinned upstream 2.4.0 source confirms
  `startTerminalTeletype()` disconnects emulation from its private master writer
  and exposes `sendData`, while `getPtySlaveFd()` returns the actual slave. The
  exact candidate has no real-adapter test, so scratch 45/45 cannot detect it.
  Candidate cannot deliver interactive keyboard input; continuing full audit.
- 2026-08-28T08:35:12-06:00 — Material P2 blocker found and posted as
  `first-party-terminal/1787927952`: locale selection in
  `terminal_launch_policy.cpp:207-224` freezes whichever locale-named entry
  appears first in envp, rather than choosing `LC_ALL > LC_CTYPE > LANG`
  independent of entry order. Thus `{LANG=en_US.UTF-8, LC_ALL=C}` is left with
  effective non-UTF-8 `LC_ALL=C`. Existing tests always place the authority
  first (`tst_launch_policy.cpp:231-314`) and miss the reversed-order case.
  Continuing the exact full-descendant audit; no product/Git/runtime action.
- 2026-08-28T08:33:28-06:00 — Self-declared live after verifying the detached
  worktree is clean at exact commit `2386e7464bcebe17dd074299ac20f1739a5bf8b1`,
  tree `e263cdd265aa2f722b7d9277dbd61d1593f258e4`, parent `f98d0e1`.
  Independently reproduced the ADR-renumber manifest SHA-256
  `cc70fe78…5be9`. Read AGENTS.md, Terminal/AppShell/module/testing/docs
  policies, the operating model/roster, Juno's prior FAIL, Sagan's preflight,
  Micah's repair and ADR-0030 handoffs, and the reserved-number allocation.
  Reviewing teardown, launch/environment, qtermwidget confinement, PTY
  ownership, presentation/accessibility, registered-test truth, docs, and
  current-main collision risk. No product/Git edits, compiler, PTY, GUI,
  session, or host-state interaction.
- 2026-08-28T08:32:02-06:00 — Manager created a detached exact-candidate
  review worktree. Source/test/docs evidence only; no compiler, PTY, GUI,
  session, host input/config, or product edit is authorized.
