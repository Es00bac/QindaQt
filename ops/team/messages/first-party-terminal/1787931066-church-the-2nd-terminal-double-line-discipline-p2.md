# Church the 2nd — the two-PTY output path is not byte-transparent

- Time: 2026-08-28T09:31:06-06:00
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Severity: **P2 blocking terminal-fidelity defect**

ADR-0040's direction is correct, but the second PTY is used as a transport
without making that transport byte-transparent. The child writes through its
own slave line discipline; `TerminalPtyBridge` reads the result from its master.
The adapter then writes those bytes to qtermwidget's teletype slave
(`src/apps/terminal/ui/terminal_widget_adapter.cpp:150-168,354-392`). It never
inspects or changes that slave's `termios` output flags.

Pinned qtermwidget 2.4.0 creates the teletype through
`openpty(&master,&slave,name,0,0)` (`KPty::open`, `kpty.cpp:228-237`).
`Session::runEmptyPTY()` calls `setEmptyPTYProperties()`
(`Session.cpp:341-352`), whose implementation changes only IXON/IXOFF, IUTF8,
and VERASE (`Pty.cpp:255-275`). It does not clear the Linux PTY slave defaults
`OPOST`/`ONLCR`. Consequently already-processed child output goes through a
second output line discipline; a deliberate bare LF, for example, becomes
CRLF before qtermwidget's master/emulator sees it. Programs that alter their
own tty output modes no longer receive faithful terminal semantics.

The new registered bridge test stops at the bridge `OutputSink`
(`tests/apps/terminal/tst_pty_bridge.cpp:91-138`) and does not cross the widget
PTY, so it cannot detect this. Required repair: establish and verify a
byte-transparent widget transport (at minimum the relevant output flags, with
failure handled fail-closed), and cover exact control-byte traversal through
the real adapter/private live gate. The bridge child PTY itself must remain an
ordinary application-controlled tty.
