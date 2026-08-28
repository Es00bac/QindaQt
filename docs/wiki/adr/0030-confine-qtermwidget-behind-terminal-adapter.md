# ADR-0030: Confine qtermwidget6 behind the Terminal rendering adapter

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Terminal application (`src/apps/terminal`)
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt has no first-party terminal (roadmap QQ-006.08). A terminal slice needs
VT/xterm emulation, UTF-8 decoding, selection, scrollback, and PTY plumbing.
Writing a new VT parser would reinvent a large, hostile, well-understood body of
work. GTK's VTE does not fit a Qt Widgets application. Konsole's internal classes
are not a supported library boundary. The repository already permits focused KDE
or platform libraries behind explicit boundaries (ADR-0007, ADR-0009), and
first-party applications may use application-focused libraries
([module boundaries](../architecture/module-boundaries.md)).

Arch/Manjaro ships `extra/qtermwidget 2.4.0-1` (LGPL-2.1-or-later, upstream
lxqt/qtermwidget): a maintained Qt 6 terminal widget providing the CMake package
`qtermwidget6`, headers under `qtermwidget6/`, and `libqtermwidget6.so`.

Upstream source at tag `2.4.0` pins the exact integration contract this slice
relies on:

- `QTermWidget::startTerminalTeletype()` opens an empty PTY whose master stays
  inside the widget and disconnects emulator-to-master keyboard writes, then
  re-exposes keyboard bytes on the `QTermWidget::sendData(const char*, int)`
  signal (`lib/qtermwidget.cpp`, `Session::runEmptyPTY`).
- `QTermWidget::getPtySlaveFd()` returns the session's slave descriptor
  (`Session` constructor, `Session::getPtySlaveFd`), so the application can run
  its own child on that slave.
- The widget's `Pty` reads the master through `readyRead` regardless of whether
  its internal `KProcess` ever starts, so child output reaches the emulator, and
  `Session::updateTerminalSize()` applies `TIOCSWINSZ` on widget resize
  independently of child presence.

## Decision

Terminal S0 uses `qtermwidget6` 2.4.x as the terminal rendering/VT dependency,
confined to one view-adapter translation unit set inside `src/apps/terminal`.
No other module may include `qtermwidget6` headers or link the library.

The adapter consumes the teletype contract above: the widget owns the PTY
master and all rendering; the application owns the child. The adapter forks,
makes the child a session leader with the slave as controlling terminal, and
`execve`s the resolved program with an argv array — never a shell string.
Because the application owns the child, it owns `waitpid` exit truth (exit
code or signal), the process-group identifier captured at start, and bounded
escalating teardown (master close, then SIGTERM, then SIGKILL to that exact
process group).

The dependency is mandatory for the terminal target and additive for the
repository: modules outside `src/apps/terminal` gain no qtermwidget include
paths, and tests use fake backends so they do not link the library. The
build enforces the audited series fail-closed with
`find_package(qtermwidget6 2.4...<2.5 REQUIRED)`: upstream publishes
`AnyNewerVersion` config version files, so the exclusive range rejects an
unaudited 2.5+ at configure time. Acceptance records that the project
commits to this dependency for the Terminal application; the dependency
itself was not provisioned on the authoring host, so executable evidence
remains the serialized compiler lane's responsibility.

## Consequences

- The smallest maintainable non-reinvented VT stack is reused instead of
  growing a parser inside QindaQt; the license surface of the installed
  application gains one LGPL-2.1-or-later dynamically linked library.
- Exit reporting is honest and complete (exit code or signal) instead of the
  best-effort event that delegating child creation to the widget would allow.
- The integration contract depends on pinned 2.4.x upstream behavior. Bumping
  qtermwidget requires re-verifying `startTerminalTeletype`, `getPtySlaveFd`,
  master-read wiring, and resize propagation at the new tag before the bump
  lands.
- CI build lanes must install the `qtermwidget` package; both Arch build jobs
  in `.github/workflows/ci.yml` gain exactly that line.
- Keyboard bytes flow through the adapter's bounded slave-forwarding buffer;
  sustained backpressure beyond the bounded buffer is dropped rather than
  blocking the GUI thread.
- Tests prove policy, session lifecycle, teardown sequencing, window semantics,
  and appearance documents through fakes; the real adapter is exercised only by
  the serialized compiler/live lane.

## Revisit when

- Upstream changes the teletype contract or drops `getPtySlaveFd`.
- A first-party need for multiplexed sessions, search, or hyperlinks justifies
  a different or additional VT boundary.
- QindaQt adopts a shared application-shell contract that changes how
  applications own platform dependencies.
