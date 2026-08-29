# Church the 2nd — Terminal repair exact rereview FAIL

- Time: 2026-08-28T09:35:08-06:00
- Reviewer: Church the 2nd (OpenAI collaboration runtime; exact serving model
  and reasoning unexposed)
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Tree: `87ed4cec98b1d8faf1a170514c29917286da108d`
- Parent / prior reviewed FAIL:
  `2386e7464bcebe17dd074299ac20f1739a5bf8b1`
- Exact repair manifest SHA-256:
  `9da026270d0080ec3bc1ac7f58e86f7661c4cc0b309ca4b1758e46361945589f`
- Verdict: **FAIL — P0/P1/P2/P3 = 0/2/3/4**

This verdict applies only to immutable commit `9bd54448`. I independently
verified its detached worktree was clean, reproduced its tree/parent and the
26-path repair manifest, read the complete descendant and every affected
Terminal source/test/doc/build path, compared every prior `0/4/5/4` finding,
and checked pinned qtermwidget 2.4.0 PTY/selection source where the public API
does not express transport semantics. No product/ref edit, configure, compile,
registered CTest, PTY, GUI, session, display, input, configuration, or host
state action occurred.

## P1-1 — real Restart→Close still starts a replacement child

`TerminalSession::beginShutdown()` now cancels `m_restartAfterShutdown` when
called in ShuttingDown (`src/apps/terminal/session/terminal_session.cpp:128-134`).
The actual window's ShuttingDown close branch does **not** call it: it sets only
`m_quitRequested`, accepts, and returns
(`src/apps/terminal/ui/terminal_window.cpp:383-387`). The pending restart stays
true, so clean completion calls `spawnGeneration()` before emitting
`shutdownFinished` (`terminal_session.cpp:204-225`), then the window emits the
queued quit signal. The ordinary Restart action → window Close interaction
still starts generation 2 immediately before application exit.

The new test explicitly substitutes `session->beginShutdown()` for “the close
path” and never closes a window
(`tests/apps/terminal/tst_terminal_session.cpp:404-424`). Repair the production
route and add a window-level regression proving one backend/widget/child.

## P1-2 — normal child exit leaves the bridge read notifier hot

`TerminalPtyBridge::pumpMasterToSink()` recognizes EIO as the child/slave exit
condition but returns without disabling `m_readNotifier`
(`src/apps/terminal/session/pty_bridge.cpp:155-177`). On Linux, after the last
slave closes, the still-open PTY master remains HUP/EIO readable; the enabled
Read notifier can immediately reactivate and return EIO again. Normal reap
stops the session poll and intentionally retains the backend/scrollback in
Exited (`terminal_session.cpp:269-288`), so the master/notifier remain alive
indefinitely. The basic shell-exit state can therefore hot-loop the GUI/CPU.

The bridge tests close the bridge immediately after their slave and never
retain an Exited backend (`tests/apps/terminal/tst_pty_bridge.cpp:136-137,158-159`).
Disable terminally dead read notification while retaining scrollback and add a
bounded event-loop liveness regression.

## P2-1 — action and selection availability remain false

The wiki says paste deactivates when no generation is live
(`docs/wiki/apps/terminal.md:111-117`), but `updateViewActionStates()` treats
any retained widget as live and excludes only ShuttingDown
(`src/apps/terminal/ui/terminal_window.cpp:251-265`). Exited retains the
scrollback widget, so paste/paste-selection remain enabled with no child. The
test stops at Running→ShuttingDown and never exercises Running→Exited
(`tests/apps/terminal/tst_terminal_window.cpp:397-442`). Keep scrollback
copy/select/clear available but gate paste on Running.

The real adapter also emits `selectionChanged(true)` unconditionally after
Select All (`terminal_widget_adapter.cpp:412-427`); a blank buffer can make
Copy enabled while `selectedText(false)` is empty. Publish actual
`hasSelectedText()` and cover the blank transition.

## P2-2 — four registered rows cannot start in headless CI

Every C++ Terminal test links `qindaqt_terminal_support`, which PUBLIC-links Qt
Widgets (`src/apps/terminal/CMakeLists.txt:42-46`), and all five sources use
`QTEST_MAIN`. Qt 6 therefore selects `QApplication`. Only the window row has
`QT_QPA_PLATFORM=offscreen` (`tests/apps/terminal/CMakeLists.txt:37-49`). The
launch-policy, PTY-bridge, session, and appearance rows attempt the default GUI
platform. The dependency-light workflow's offscreen variable is scoped only to
preview smoke; the following headless CTest step has none
(`.github/workflows/ci.yml:95-103`). Those registered executables can abort in
platform initialization before tests run. Micah's desktop scratch harness
cannot qualify this. Give every Widgets/GUI-linked row an explicit offscreen
environment (or truly Core-only link/main boundary) and prove the registered
selector with DISPLAY/WAYLAND_DISPLAY absent.

## P2-3 — the second PTY post-processes output again

The bridge reads already line-disciplined child output from its master, then
the adapter writes it into qtermwidget's teletype **slave**
(`src/apps/terminal/ui/terminal_widget_adapter.cpp:150-168,354-392`) without
making that transport byte-transparent. Pinned qtermwidget 2.4.0 creates the
PTY using `openpty(..., 0, 0)`; `runEmptyPTY()` changes IXON/IXOFF, IUTF8 and
VERASE only, not the Linux default output flags OPOST/ONLCR. Thus a deliberate
bare LF, for example, is transformed to CRLF by the second PTY, changing the
semantics of programs that configure their child tty output mode.

The new bridge row ends at `OutputSink` and never crosses the widget PTY
(`tests/apps/terminal/tst_pty_bridge.cpp:91-138`). Configure the widget
transport as byte-transparent with fail-closed error handling, while leaving
the child PTY ordinary, and exercise exact control bytes through the real
adapter gate.

## P3 findings

1. Program/working-directory diagnostics promise byte bounds but compare
   UTF-16 `QString::size()` (`terminal_launch_policy.cpp:115-120,191-197`), so
   multibyte paths can exceed the claimed ceiling.
2. `close_range` errors other than ENOSYS/EINVAL return without executing the
   bounded fallback (`terminal_widget_adapter.cpp:77-89`); a sandbox EPERM can
   leak inherited descriptors across exec. Signal-mask reset is fixed.
3. The supposedly symlink-safe scheme still opens a predictable
   `pid-counter.tmp` with Truncate (`terminal_widget_adapter.cpp:217-243`), so
   a pre-existing temp symlink redirects truncation; a crash-stale target plus
   PID reuse also makes rename fail instead of replace atomically.
4. Accepted ADR-0040 says the child opens the slave after `setsid()`
   (`docs/wiki/adr/0040-own-terminal-child-pty-and-bridge-through-teletype.md:32-34`),
   and the source comment repeats that order, but implementation opens at
   adapter line 101, calls setsid at 106, and TIOCSCTTY at 110. Make the durable
   architecture/comment and actual conventional ordering agree.

## Prior findings repaired at this stopping point

- The application-owned bridge fixes former P1-1's fatal keyboard direction:
  keyboard/paste write the child PTY master; child output is read from that
  master and forwarded toward the renderer. The child opens its own slave open
  description, so adapter O_NONBLOCK does not leak into child stdio.
- Former P1-2 is fixed: failed escalation retains backend/PID, refuses
  start/restart/shutdown/close, re-shows the window, and never emits the clean
  quit signal. Former P1-4's one-past **row** is fixed; real extraction remains
  a private live-gate caveat.
- Locale authority now follows LC_ALL→LC_CTYPE→LANG independent of envp order,
  uses a strict codeset match, and forces exact fallback; inherited TERM and
  COLORTERM remain fully removed/replaced.
- argv/envp pointer arrays are built before fork; setup errors, signal-mask
  reset, and Linux-wide descriptor close are present subject to P3-2.
- qtermwidget is an exclusive `2.4...<2.5` required package and a PRIVATE
  adapter dependency; only the adapter cpp includes its header.
- ECHILD publishes typed UnknownExit rather than normal zero. ADR-0030 is
  properly preserved as Superseded, ADR-0040 is Accepted, navigation/links are
  consistent, and no stale Terminal ADR-0028/0029 reference remains.
- Production files stay below 500 nonblank lines (largest Terminal production
  file: adapter, 413). The registry declares exactly eight unique
  `qindaqt.terminal-*` rows.

## Independent evidence and caveats

- `git diff --check`: exit 0.
- `tools/check-source-shape`: exit 0, 1030 files.
- `tools/validate-docs`: exit 0, 66 documents/navigation.
- `mkdocs build --strict`: unavailable in this review environment (`mkdocs`
  command absent and `python3 -m mkdocs` has no module). Micah reports a
  scratch pass; it is not relabelled independent evidence.
- No executable evidence is credited. Micah's qtermwidget-free scratch harness
  is useful but is not the eight registered rows and cannot observe the real
  adapter, P1-1, P1-2, or P2-3.
- Current public moved during review to origin/main `691847333c0b7ac6fd4a838ed0c1021c1e098281`
  (ADR-0041 Flow workflow). Read-only `merge-tree` still reports exactly three
  textual conflicts: ADR index, wiki index, and MkDocs navigation; module
  boundaries plus source/test registries auto-union. Integration must preserve
  current public/AppShell/PB-0/Virtual Desktop/ADR-0041 and candidate Terminal/
  ADR-0030/ADR-0040. Any additional conflict after a new public move requires a
  fresh preflight.

## Required next action

Do not integrate or send `9bd5444` to the compiler/private runtime gate. Route
these exact reproductions to Micah Stone, preserve this candidate, and return a
clean non-amended descendant to Church for the same-reviewer rereview. After a
source PASS, provision qtermwidget 2.4.x, run all eight registered rows in the
serialized compiler lane plus combined-current-public regressions, then one
private real-adapter round trip for keyboard→child, control-byte/UTF-8 output,
resize/SIGWINCH, select/copy, exit/HUP-notifier quiescence, close,
Restart→Close, failed-escalation ownership, first frame, and PSS. Church remains
available.
