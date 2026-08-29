# Dijkstra the 2nd — Terminal exact rereview P1 findings

- Time: 2026-08-28T17:00:55Z
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Severity/count at this checkpoint: two P1 blockers
- Product edits: none

## P1 — Restart then real window Close still launches generation 2

`TerminalSession::beginShutdown()` cancels `m_restartAfterShutdown` only when
that method is called in ShuttingDown (`src/apps/terminal/session/terminal_session.cpp:128-134`).
The actual `TerminalWindow::closeEvent()` ShuttingDown branch sets only
`m_quitRequested`, accepts the event, and returns
(`src/apps/terminal/ui/terminal_window.cpp:383-387`). On clean completion,
`TerminalSession::completeShutdown()` therefore observes the still-true restart
flag and calls `spawnGeneration()` before emitting `shutdownFinished`
(`terminal_session.cpp:204-225`); the window then emits the queued quit seam.

The new test does not exercise this production route: it calls
`session->beginShutdown()` directly and labels that substitute "The close
path" (`tests/apps/terminal/tst_terminal_session.cpp:404-424`). No window-level
Restart→Close regression exists. The ordinary action interaction can start a
fresh child immediately before application exit.

## P1 — retained normal exit can hot-loop on bridge PTY EIO/HUP

`TerminalPtyBridge::pumpMasterToSink()` drains readable bytes, but every
terminal read outcome—including Linux `EIO` after the last slave closes—returns
without disabling `m_readNotifier`
(`src/apps/terminal/session/pty_bridge.cpp:155-177`). Linux keeps a still-open
PTY master HUP/EIO-readable after the slave side disappears. Normal reap stops
the session poll and retains the backend/widget for Exited scrollback
(`src/apps/terminal/session/terminal_session.cpp:269-288`), so the bridge and
enabled notifier remain alive and can immediately reactivate indefinitely.

The bridge tests close the bridge immediately after their slave and never hold
an Exited backend through bounded event-loop iterations
(`tests/apps/terminal/tst_pty_bridge.cpp:91-138,140-160`). The source needs a
terminally-dead read-notifier state plus a retained-scrollback liveness
regression.

These findings alone prevent integration. The complete prior-finding audit and
proportional static/runtime evidence continue so the final immutable verdict is
not a stopping-point extrapolation.
