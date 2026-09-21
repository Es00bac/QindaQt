# Terminal (QQ_Term)

QindaQt's terminal is **QQ_Term**. It is not part of this repository: it lives
in the [QindaQt_Apps](https://github.com/Es00bac/QindaQt_Apps) repository and
ships as the Portage package `gui-apps/qqterm`, which `gui-wm/qindaqt-desktop`
depends on at runtime. The `qindaqt-terminal` application that used to be built
here was removed in [ADR-0222](../adr/0222-qq-term-is-the-desktop-terminal.md);
this page records only what the desktop itself contracts with.

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
- **Settings1.** QQ_Term reads the desktop's appearance and accessibility
  settings through the public [Settings1](../reference/settings1-v1.md) client;
  `accessibility.reducedTransparency` clamps its per-profile opacity to opaque.
- **Icon theme.** The QindaQt icon theme aliases `qqterm` and
  `org.qindaqt.QQTerm` onto the shared terminal glyph
  (`tools/qinda_icon_catalog_apps.py`), so the first-party terminal looks like
  the rest of the desktop. See [Shell iconography](../shell/iconography.md).

## Profiles and Settings1 persistence

QQ_Term owns its own profile store (theme, font, opacity, scrollbar,
scrollback, keytab, shell, working directory) under
`~/.config/QindaQt/qqterm`, and a per-window profile can be switched live from
its Profiles menu. Desktop-wide appearance and accessibility values still
arrive through Settings1; the terminal never writes them. The complete profile,
theme-rotation, and colour-scheme behaviour is documented in QQ_Term's own
README in the QindaQt_Apps repository.

## Related

- [ADR-0222: QQ_Term is the desktop terminal](../adr/0222-qq-term-is-the-desktop-terminal.md)
- [ADR-0112: terminal protocol palette](../adr/0112-terminal-protocol-palette.md)
- [Launcher](../shell/launcher.md)
- [Task list source model](../shell/task-list.md)
- [First-party applications](../handbook/applications.md)
