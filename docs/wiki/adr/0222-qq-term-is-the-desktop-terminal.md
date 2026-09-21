# ADR-0222: QQ_Term is the desktop terminal

- **Status:** Accepted
- **Date:** 2026-09-20
- **Owners:** First-party applications

## Context

This repository built `qindaqt-terminal`: about 8100 lines carrying its own
child-PTY bridge, teletype lifecycle, ANSI palette, scrollback search, link
presentation, and profile dialogs. QQ_Term was then written in the
[QindaQt_Apps](https://github.com/Es00bac/QindaQt_Apps) repository on top of
`x11-libs/qtermwidget`, packaged as `gui-apps/qqterm`, and installed alongside
it. The desktop therefore shipped two first-party terminals with two desktop
entries, two icons, and two launcher rows, and the emulator QindaQt maintained
by hand was the weaker of the two.

The operator directed that QQ_Term replace it on both machines and that
`qindaqt-terminal` be removed.

## Decision

`gui-apps/qqterm` is the desktop's terminal. `src/apps/terminal` and
`tests/apps/terminal` are removed from this repository, along with the
`Terminal` install component, the `qindaqt-terminal` release-contract entry,
and the `org.qindaqt.Terminal` desktop entry and icon alias.

- `gui-wm/qindaqt-desktop` gains a runtime dependency on `gui-apps/qqterm`, so
  a desktop install always has a terminal.
- The desktop context menu's *Open Terminal Here* launches
  `org.qindaqt.QQTerm` through the same launcher seam as every other entry.
- The QindaQt icon theme aliases `qqterm` and `org.qindaqt.QQTerm` onto the
  shared terminal glyph, so the replacement keeps the desktop's look.
- QQ_Term gains an XTerm-compatible `-e PROGRAM [ARG...]` form. The flag and
  everything after it are split off before `QCommandLineParser` runs, so the
  command keeps its own options; `argv[0]` becomes the session program and the
  rest its arguments, so the command is still never shell-interpreted.
  `--shell` and `-e` are mutually exclusive, and a bare command name is
  resolved through `PATH` because that is what a `Terminal=true` `Exec` key
  contains.
- `ShellRuntimeApplication` wires `{"qqterm", "-e"}` as `LaunchExecutor`'s
  terminal command prefix. That prefix was empty in production, so **every**
  `Terminal=true` desktop entry refused to launch; the refusal was truthful
  but the capability was missing. It now works, and an absent QQ_Term still
  fails with a diagnostic rather than falling back to another terminal or to a
  shell.

## Consequences

The desktop has one terminal, one desktop entry, one icon, and one launcher
row. This repository sheds 8100 lines of hand-maintained terminal emulation
and its test surface; the emulator is `qtermwidget`'s, maintained upstream.
`Terminal=true` entries launch for the first time.

The cost is a cross-repository boundary: the terminal's behaviour is no longer
pinned by this repository's test suite, and `docs/wiki/apps/terminal.md` now
records only the integration contract (executable name, desktop entry,
`StartupWMClass`, icon name, bus name, and the `-e` form). A change to any of
those six values on either side is a breaking change.

The private Gabbee terminal lanes staged the `Terminal` install component; they
now resolve the installed `qqterm` and mount its prefix, and fail with a clear
message when the package is absent. `QQ-006.08` keeps its `EXECUTABLE` state
because the capability still ships — by a different implementation, recorded in
its evidence.
