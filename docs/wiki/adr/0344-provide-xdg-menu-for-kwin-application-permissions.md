# ADR-0344: Provide an XDG applications menu for KWin permissions

Status: Accepted on 2026-10-03.

## Context

QindaQt's KWin fork resolves a caller's installed desktop entry with
`KApplicationTrader` before granting restricted interfaces such as
`org.qindaqt.KWin.ScreenShot2`. Removing Plasma left
`/etc/xdg/menus/applications.menu` as a dangling link to
`plasma-applications.menu` on qinda-top. With no usable applications menu,
`KApplicationTrader` returned zero services even though
`org.qindaqt.Screenshot.desktop` was installed and correctly declared its
permission. The Print shortcut reached Screenshot, but KWin denied capture.
The existing private capture fixtures already supplied a minimal menu for
this same service lookup.

## Decision

The `qindaqt` Portage overlay provides `x11-misc/qindaqt-xdg-menu`, which owns
`/etc/xdg/menus/applications.menu`. It includes the standard application and
directory roots and merged menus. The QindaQt systemd profile and exact shared
delivery include the package. A running session rebuilds its KService cache
with `kbuildsycoca6 --noincremental` after the package is first installed;
subsequent sessions find the system menu without a per-user generated file.

## Consequences

KWin can resolve the installed Screenshot desktop entry and its exact
restricted-interface declaration. The same application catalog lookup is
available to other KService consumers after Plasma removal. Portage owns the
default under `/etc`; an administrator's local menu edits remain subject to
Portage config protection. A desktop package that installs a competing
`applications.menu` must be reconciled explicitly rather than silently
replacing this file.

This does not change Screenshot's capture API or the fork's authorization
rule in [ADR-0340](0340-use-native-privileged-compositor-identities.md).
