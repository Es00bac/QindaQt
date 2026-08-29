# Church the 2nd — Terminal S0 P1: keyboard bytes are written to the PTY slave, not input to the child

- Time: 2026-08-28T08:37:58-06:00
- Owner: Church the 2nd
- Addressees: Micah Stone; Program Manager
- Exact candidate: `2386e7464bcebe17dd074299ac20f1739a5bf8b1`
- Severity: P1; Terminal is non-interactive and the pinned adapter architecture needs repair

The real adapter cannot deliver keyboard input to the terminal child. It
duplicates `QTermWidget::getPtySlaveFd()` into `m_slaveFd` at
`src/apps/terminal/ui/terminal_widget_adapter.cpp:126-139`, connects emitted
keyboard bytes at lines 143-146, and `forwardKeyboardBytes()` /
`flushKeyboardBuffer()` writes those bytes to that slave at lines 267-309.
The forked child also adopts the same slave for stdin/stdout/stderr at lines
69-85 and 229-239.

Unix PTY direction is not symmetric for this purpose: terminal input is
written to the **master** and read by the process from the slave; output is
written to the slave and read by the master. Writing qtermwidget's emitted
keyboard bytes back to the slave makes them output toward the widget/emulator;
it does not put them into the child's slave stdin. A launched shell therefore
cannot receive ordinary typing, paste, or `sendTextToSession()`.

The exact pinned upstream qtermwidget 2.4.0 source confirms the mismatch:

- `QTermWidget::startTerminalTeletype()` calls `Session::runEmptyPTY()` and
  exposes emulation `sendData` to the external recipient
  ([upstream qtermwidget.cpp at tag 2.4.0](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/qtermwidget.cpp#L280-L290)).
- `Session::runEmptyPTY()` explicitly disconnects emulation `sendData` from
  its internal `Pty::sendData`, the object that writes the private master
  ([upstream Session.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Session.cpp#L341-L353)).
- `getPtySlaveFd()` returns `_shellProcess->pty()->slaveFd()`, not the master
  ([upstream Session.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Session.cpp#L80-L83)).
- Upstream `Pty::sendData()` is the master-side writer used during normal
  operation ([upstream Pty.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Pty.cpp#L333-L343)).

The four support tests deliberately do not link the adapter, and the seven
registered rows contain no live qtermwidget/PTY keyboard round trip. Micah's
scratch 45/45 support result therefore cannot detect this production failure.
The repair must provide a real master-side input path while preserving one
clear child/PTY owner, and must add an isolated real-adapter round trip proving
typed bytes reach the child's stdin and child output reaches the emulator.
Simply retrying or buffering writes to the slave cannot repair direction.

I am continuing the remaining exact audit. No product/Git mutation, compiler,
PTY, GUI, session, host input/config, or live runtime occurred in this review.
