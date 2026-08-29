# Church the 2nd — exited child leaves the PTY read notifier hot

- Time: 2026-08-28T09:31:48-06:00
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Severity: **P1 blocking exit/resource behavior**

`TerminalPtyBridge::pumpMasterToSink()` handles positive reads and EINTR, then
explicitly treats `EIO` as “slave side has no open descriptor (child exited)”
but only returns (`src/apps/terminal/session/pty_bridge.cpp:155-177`). It does
not disable `m_readNotifier` or transition the bridge into an output-closed
state.

On Linux, once the last PTY slave descriptor closes, the still-open master
remains hangup-readable and reads report EIO after buffered data drains. The
enabled `QSocketNotifier::Read` therefore continues to activate and the same
callback immediately returns EIO again. Normal child exit stops only the
session's 20 ms process poll and enters `Exited`
(`terminal_session.cpp:269-288`); the backend is intentionally retained to
show scrollback, so its bridge master and read notifier remain alive. The
ordinary “shell exits, leave scrollback open” state can consequently peg the
GUI event loop/CPU until restart or close.

The bridge tests close the bridge immediately after closing their slave
(`tests/apps/terminal/tst_pty_bridge.cpp:136-137,158-159`) and never keep an
Exited backend through another event-loop turn. Required repair: distinguish
EAGAIN from EOF/EIO/hard errors, permanently disable the read notifier when the
generation's slave is terminally gone while retaining rendered scrollback, and
add a bounded notifier/liveness regression for slave close. No PTY executable
was launched in this source-only review.
