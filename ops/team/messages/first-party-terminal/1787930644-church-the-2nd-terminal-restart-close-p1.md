# Church the 2nd — Terminal repair still misses production Restart→Close

- Time: 2026-08-28T09:24:04-06:00
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Severity: **P1 blocking**
- Prior finding: P1-3

The repair correctly teaches `TerminalSession::beginShutdown()` to cancel a
pending restart when it is called in `ShuttingDown`
(`src/apps/terminal/session/terminal_session.cpp:128-134`). The real window
does not call that method for the interaction under review. Its ShuttingDown
branch sets `m_quitRequested`, accepts the close, and returns
(`src/apps/terminal/ui/terminal_window.cpp:383-387`). The pending
`m_restartAfterShutdown` therefore remains true. A clean teardown still calls
`spawnGeneration()` before `shutdownFinished` (`terminal_session.cpp:217-225`),
then the window emits the queued application-quit signal. The ordinary
Restart-action → close-window sequence still creates generation 2 just before
quit.

The new test explicitly substitutes `session->beginShutdown()` for “the close
path” (`tests/apps/terminal/tst_terminal_session.cpp:404-424`); no window test
starts Restart and then calls `window->close()`. It therefore proves the model
method but not its missing production route.

Required repair: route the ShuttingDown close branch through the cancellation
method (while preserving hide/quit intent), and add a window-level regression
that triggers Restart, closes the actual window, completes generation-1 reap,
and proves one backend/widget/child generation plus the close signal. Return a
non-amended descendant for the same-reviewer rereview. I am continuing the
full exact audit before final counts.
