# Terminal (QQ_Term)

QindaQt's terminal is **QQ_Term**. It is not part of this repository: it lives
in the [QindaQt_Apps](https://github.com/Es00bac/QindaQt_Apps) repository and
ships as the Portage package `gui-apps/qqterm`, which `gui-wm/qindaqt-desktop`
depends on at runtime. The `qindaqt-terminal` application that used to be built
here was removed in [ADR-0222](../adr/0222-qq-term-is-the-desktop-terminal.md);
this page records the desktop integration contract and current audit status.
Since QQ_Term 0.3.0 its UI uses QindaTK/Qt Quick and the terminal engine is
QindaTK's libvterm-backed `TerminalSession`; qtermwidget describes the historical
replacement in ADR-0222, not the current implementation.

## What the desktop expects from it

| Contract | Value |
| --- | --- |
| Executable | `qqterm` |
| Desktop entry | `org.qindaqt.QQTerm.desktop` |
| `StartupWMClass` | `qqterm` |
| Icon name | `qqterm` |
| Single-instance bus name | `org.qindaqt.QQTerm` |
| Run-one-command form | `qqterm -e PROGRAM [ARG...]` |

One session per window, no tabs: the desktop's
[window containers](../architecture/window-containers.md) supply tabs and
splits by grouping separate windows, so the terminal does not reimplement them.

## Where the desktop reaches it

- **Desktop context menu.** The traditional desktop style's *Open Terminal
  Here* entry launches `org.qindaqt.QQTerm` through the launcher seam
  (`DesktopContextMenu.qml`), like every other launch entry — never a direct
  process start.
- **Terminal launch policy.** `Terminal=true` desktop entries run inside
  QQ_Term. `ShellRuntimeApplication` wires `{"qqterm", "-e"}` as
  `LaunchExecutor`'s terminal command prefix; the executor appends the entry's
  own program and arguments, and `-e` takes them verbatim rather than through a
  shell string. An absent QQ_Term makes the spawn fail with a truthful
  diagnostic — there is no fallback to another terminal and never a shell
  fallback (see [Launcher](../shell/launcher.md)).
- **Global menu.** QQ_Term exports its window menu as a standard
  `com.canonical.dbusmenu` endpoint and announces it per window over the KDE
  appmenu Wayland protocol, so the panel's
  [global menu](../shell/global-menu.md) shows the focused terminal's menu.
  The export is fail-closed: without it the in-window menu bar stays.
- **Desktop appearance.** QQ_Term imports QindaTK's `QindaQtTheme` bridge.
  Profiles own terminal appearance. Full accessibility policy conformance,
  including reduced-transparency behavior, needs current implementation evidence;
  the old qtermwidget-era claim is not a current acceptance receipt.
- **Icon theme.** `qqterm` (and `org.qindaqt.QQTerm`) has its own QQ_Term
  mark — a dark console with a plum title strip and an amber double prompt —
  in `tools/qinda_icon_catalog_first_party.py`, distinct from the generic
  `utilities-terminal` glyph other terminals use
  ([ADR-0283](../adr/0283-true-color-places-types-and-qinda-app-marks.md)).
  See [Icon theme](../shell/icon-theme.md).

## Profiles and terminal interactions

The current profile store is `~/.config/qqterm/qqterm.ini`; user color schemes
live under `~/.config/qqterm/color-schemes`. Profiles store color scheme, font
scale, shell, working directory, opacity, blur and bounded scrollback. The
Profiles menu opens a new window on the selected profile. Saving appearance
changes in Preferences updates windows sharing that profile; interactive zoom
is window-local in 0.3.1 and resets to the saved profile scale.

The 0.3.1 application repairs add regex search with Unicode-aware selection,
visible match/error status, web/email link preview and explicit Open/Copy
handling, quoted local-file drops, multiline-paste review and a read-only Qt
accessibility text interface. Automated accessibility checks cover readable
text, selection and geometry; native screen-reader qualification remains open.
The source README and `docs/terminal-parity.md` in QindaQt_Apps own the detailed
behavior and acceptance commands.

## November 2 parity audit

**Full parity with GNOME Terminal/Console, KDE Konsole, Xfce Terminal and MATE
Terminal is not established.** Four application regression groups pass, but
an independent engine probe demonstrates that narrowing a live screen can
crop printed text and that copying wrapped commands inserts physical newlines.
The 10,000-line history cap, missing OSC 8 metadata, configurable shortcuts and
advanced selection/shaping also remain gaps.

QindaTK experiment `0a165c57` is preserved on hub branch
`codex/terminal-text-20261102` and explicitly rejected. Simply enabling
libvterm 0.3.3 reflow preserves short ASCII examples but aborts when a logical
line spans history (exit 134), and wide-glyph reflow introduces extra spaces.
It is neither integrated nor packaged. The repair must preserve logical lines,
styles, spaces and cursor state across history and primary/alternate buffers;
passing ordinary shell/input tests is insufficient. No feature-completion
credit or full-parity milestone is warranted.

## Related

- [ADR-0222: QQ_Term is the desktop terminal](../adr/0222-qq-term-is-the-desktop-terminal.md)
- [ADR-0112: terminal protocol palette](../adr/0112-terminal-protocol-palette.md)
- [Launcher](../shell/launcher.md)
- [Task list source model](../shell/task-list.md)
- [First-party applications](../handbook/applications.md)
