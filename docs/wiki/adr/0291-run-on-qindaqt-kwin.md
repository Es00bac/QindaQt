# ADR-0291: Run QindaQt on qindaqt-kwin, its own co-installable KWin fork

- **Status:** Accepted
- **Date:** 2026-09-28
- **Owners:** Compositor, Platform integration
- **Supersedes:** [ADR-0001](0001-use-kwin-as-compositor-base.md)'s small-downstream-patch model (KWin stays the code base) and the stock-KWin half of [ADR-0098](0098-gate-releases-on-the-exact-native-compositor-stack.md)'s exact stack
- **Amends:** [ADR-0160](0160-select-installed-kwin-window-decorations.md) (only fork-namespace decorations are offered), [ADR-0277](0277-theme-choice-and-corner-tab-input.md) and [ADR-0287](0287-pass-tablet-proximity-through-window-decorations.md) (their patches are fork commits), [ADR-0209](0209-bridge-window-management-settings-into-kwinrc.md) (the bridge writes `qindaqt/kwinrc`)
- **Superseded by:** None

## Context

The owner's goal is to replace KDE and Plasma completely, keeping a stock KDE
Plasma installable beside QindaQt, and to give QindaQt native parts designed for
QindaQt rather than renamed KDE copies. QindaQt ran on the overlay's patched
`kde-plasma/kwin`: it shared `kwinrc`, `kcminputrc`, `kxkbrc`, the
`org.kde.KWin` bus name and the `kwin/plugins` namespace with KDE, so a Plasma
upgrade could break the desktop, KDE settings leaked into QindaQt and back, and
QindaQt's plugin loaded into any stock KWin (a stock `kwin_wayland --virtual`
on qinda activated `org.qindaqt.Settings1` through it). The Plasma-free plan
(`docs/plans/2026-09-28-plasma-free-qindaqt.md`, milestone M1) chose a renamed,
co-installable fork.

## Decision

**The fork.** QindaQt's compositor is `qindaqt-kwin`: KWin's history from
`v6.6.6` plus labelled `qindaqt:` commits, hub `qinda:~/git/qindaqt-kwin.git`,
packaged as `gui-wm/qindaqt-kwin`. Its version is the upstream release plus a
fork serial, `6.6.6.1` (package `6.6.6_p1`). Upstream enters only as recorded
merges of release tags; the idempotent `qindaqt/tools/rename-identity` is re-run
after each. `compositor/upstream/kwin.json` pins it:

| Pin | Value |
|---|---|
| Fork version | 6.6.6.1 |
| Initial fork commit (September 28) | `0dd2fdb802c6dfdecb4771942b05788a8aa386b5` (tree `97ade09fdc536f6293b74d2114aaf04a699e9351`) |
| Current development fork commit | `68c4d74f903b7e8990dd5fd5d509ec8154eac1d1` (tree `9165a8817dfe190bfed59b20e42acc6291d82a27`); source qualification, installed release gates remain |
| Upstream release | KWin 6.6.6, tag object `43cb730ca363b995dfd5f0ceb537e4c37a7bb5ff`, commit `9bf2235fad10de9048c634e376bf12e56b3023e6`, tree `88f96f8cde49c51552d82f60fd461b6e8b950685` |

**The contract QindaQt consumes.** container-wm names the fork only through
`QindaQt::CompositorNames` (`src/compositor_names`):

| What | Name |
|---|---|
| Program | `/usr/bin/qindaqt-kwin`; `--version` prints `kwin 6.6.6.1` |
| CMake | `QindaQtKWin` (`QindaQtKWin::kwin`) and `QindaQtKWinDecoration` (`QindaQtKWinDecoration::KDecoration`), both `EXACT` 6.6.6.1 |
| Compositor plugin | namespace `qindaqt-kwin/plugins`, interface id `org.qindaqt.kwin.PluginFactoryInterface6.6.6.1` |
| Decorations | namespace `qindaqt-kwin/decorations`; default and fallback plugin `org.qindaqt` |
| Window switcher | `/usr/share/qindaqt-kwin/tabbox/`, structure `QindaQtKWin/WindowSwitcher`, QML `org.qindaqt.kwin`; default layout `qindaqt` |
| Config | QindaQt's folder: `$XDG_CONFIG_HOME/qindaqt/kwinrc`, `kwinrulesrc`, `kwinoutputconfig.json`, `kwininputrc`, `kwinxkbrc`; state `$XDG_STATE_HOME/qindaqt/kwinstaterc` |
| D-Bus | `org.qindaqt.KWin` (also `.NightLight`, `.HighlightWindow`, `.Effect.WindowView1`), objects under `/org/qindaqt/KWin`, interfaces `org.qindaqt.KWin*` |
| Privileged clients | desktop-file keys `X-QindaQt-KWin-Wayland-Interfaces`, `X-QindaQt-KWin-DBus-Restricted-Interfaces` |
| Notifications | component `qindaqt-kwin` |

Carve-outs until PF21: the fork also owns `org.kde.KWin`; ScreenShot2, the EIS
remote-desktop and input-capture objects, TabletModeManager (`/org/kde/KWin`),
VirtualKeyboard (`/VirtualKeyboard`) and Scripting (`/Scripting`,
`/Scripting/Script<N>`, also under the new names) keep their KDE names for the
KDE portal, Spectacle, Kirigami and Gabbee's focus backend, and the `X-KDE-*`
keys are honoured too. Gabbee moves to `org.qindaqt.KWin` before PF21. The application
and KGlobalAccel component name stays `kwin` until M6, because KF6GlobalAccel
files KWin's shortcuts under it in the shared `kglobalshortcutsrc`.

**QindaQt's defaults are the fork's defaults.** The QindaQt decoration, the
`qindaqt` switcher, disabled electric-border maximize and tiling, CommandAll3
"Nothing" (the shell's Meta+right-click chord) and the Theming v2 blur strengths
are compiled into the fork; `SessionDefaults` seeds only installation-dependent
values.

**Moving in.** `qindaqt-wm` copies each KDE file (`kwinrc`, `kwinrulesrc`,
`kwinoutputconfig.json`, `kcminputrc`, `kxkbrc`, `kwinstaterc`) to its
qindaqt-kwin name once, only while the qindaqt-kwin file does not exist; KDE's
files are never written. Only `qindaqt-kwin` starts the physical session.

**Native replacements inside the fork.** kdecoration is vendored as
`libqindaqt-kwin-decorations` (KWin links its private library, whose ABI moves
with Plasma). Ending a hung application owned by root or another user no longer
uses KAuth and ksysguard's helper: the prompt runs the fork's Qt-free
`terminate-process` through pkexec under the polkit action
`org.qindaqt.kwin.terminate-process` (always `auth_admin`), which addresses the
process by pidfd and start time so a reused PID is never signalled.

**Shared on purpose.** kwayland and layer-shell-qt (stable client libraries),
KDE Frameworks, `kdeglobals`, Wayland protocol names and `org.kde.kglobalaccel`.

## Consequences

- A stock `kde-plasma/kwin`, `kwin-x11` and Plasma install beside QindaQt:
  `qindaqt/tools/check-install-collisions` finds no shared path, and stock KWin
  no longer loads QindaQt's plugin or decoration.
- Every QindaQt D-Bus client, config writer, test fake and nested harness moved
  to the fork's names; a stock KWin cannot host the QindaQt session.
- The overlay drops its patched `kde-plasma/kwin` when `qindaqt-desktop` depends
  on `=gui-wm/qindaqt-kwin-6.6.6_p1:=`; `tools/check-release-contract
  --desktop-ebuild` checks that ebuild.
- CI's native-release lane and `verify-kwin-source --check-remote` need a fork
  source they can reach; until the fork is published they cannot pass.
- Each fork release bumps the serial, the manifest, this pin table and
  `QindaQtKWinAbi.cmake` together, and rebuilds every plugin.

## Qualification

The [compositor session contract](../architecture/compositor-session.md#m1-staged-qualification-2026-09-29)
records the 468-file staged collision proof, simultaneous private stock/fork
runtime test and native consumer plugin/decoration checks. Installation and
physical-session qualification remain manager-held until final delivery.

## Revisit when

PF21 ends the carve-outs, M6 replaces the shortcut library and the application
name, or a move to KWin 6.7 or later is planned.
