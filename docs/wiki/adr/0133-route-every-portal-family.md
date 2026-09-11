# ADR-0133: Route every portal family explicitly

- **Status:** Accepted
- **Date:** 2026-09-11
- **Owners:** Portal platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt implements exactly one portal backend itself:
`org.freedesktop.impl.portal.Settings` (ADR-0054). The frontend selector
`qindaqt-portals.conf` historically routed only the families someone had
thought about: the Settings row, the `kde;gtk;lxqt` fallback rows from
ADR-0059, GlobalShortcuts to `kde` from ADR-0086, and the closed Background
row. Everything else was closed only by the `default=none` catch-all, which is
safe but silent: no routing decision or evidence existed for Secret,
InputCapture, Clipboard, Account, Usb, DynamicLauncher, or Wallpaper, so
screen sharing consumers, screenshot callers, and Flatpak/GTK applications had
no tested story, and password storage for sandboxed applications had no route
at all.

The installed backend metadata (xdg-desktop-portal 1.20.4 stack) advertises
these implementation interfaces:

| Family | kde | gtk | lxqt | gnome-keyring |
| --- | --- | --- | --- | --- |
| Access | yes | yes | yes | — |
| Account | yes | yes | — | — |
| AppChooser | yes | yes | — | — |
| Background | yes | — | — | — |
| Clipboard | yes | — | — | — |
| DynamicLauncher | yes | yes | — | — |
| Email | yes | yes | — | — |
| FileChooser | yes | yes | yes | — |
| GlobalShortcuts | yes | — | — | — |
| Inhibit | yes | yes | — | — |
| InputCapture | yes | — | — | — |
| Notification | yes | yes | — | — |
| Print | yes | yes | — | — |
| RemoteDesktop | yes | — | — | — |
| ScreenCast | yes | — | — | — |
| Screenshot | yes | — | — | — |
| Secret | — | — | — | yes |
| Settings | yes | yes | — | — |
| Usb | yes | — | — | — |
| Wallpaper | yes | — | — | — |

The Secret Service provider decision itself is owned by the keyring workgroup
decision (ADR-0135); this ADR owns only the routing row. The live frontend
exports an interface only when the selector resolves an implementation for it,
so a missing row does not merely pick a worse backend — it removes the
user-visible interface entirely.

## Decision

Keep `default=none` so every family must have an explicit row, and route every
family found in the installed `.portal` metadata as follows:

| Family | Routing row | Reason |
| --- | --- | --- |
| Settings | `qindaqt` | The only QindaQt-implemented backend (ADR-0054) |
| Access, AppChooser, Email, FileChooser, Inhibit, Notification, Print, Screenshot, ScreenCast, RemoteDesktop | `kde;gtk;lxqt` | Existing reviewed fallback order (ADR-0059); all three advertise each family |
| GlobalShortcuts | `kde` | Only installed provider advertising it (ADR-0086) |
| Secret | `gnome-keyring` | The adopted Secret Service provider (ADR-0135 owns the provider choice); `kwallet` also advertises Secret but is not adopted |
| InputCapture | `kde` | Only provider advertising it; KDE/KWin path kept alive by the ADR-0088 identity drop-in |
| Clipboard | `kde` | Only provider advertising it |
| Usb | `kde` | Only provider advertising it |
| Account | `kde` | KDE is the primary provider in a KWin session; gtk's row adds a second, unreviewed consent surface without user benefit, so the row is deliberately narrower than the historic `kde;gtk` pair |
| DynamicLauncher | `kde` | Same reasoning as Account |
| Wallpaper | `none` | The KDE backend would change Plasma's wallpaper, which QindaQt does not use; QindaQt owns wallpaper surfaces in the shell (ADR-0078) |
| Background | `none` | Closed on purpose (ADR-0059) |

Frontend-implemented interfaces (OpenURI, Trash, Camera, GameMode,
NetworkMonitor, PowerProfileMonitor, MemoryMonitor, ProxyResolver, Realtime)
have no backend selector and therefore no row.

## Consequences

- `qindaqt-portals.conf` gains the six new rows; exact-byte source, staged, and
  poison tests pin every row, so dropping or rerouting one fails closed.
- A private-bus proof drives the real `xdg-desktop-portal` frontend with fake
  `kde` and `gnome-keyring` backends and asserts FileChooser, Screenshot,
  ScreenCast, RemoteDesktop, InputCapture, and Secret requests reach the
  routed backend; Wallpaper and Background stay unexported; a selector without
  the Secret row removes the Secret interface (mutation control).
- Real-backend coverage remains bounded: the optional headless smoke records
  how far a non-interactive Screenshot and a ScreenCast session get; dialogs
  that need a human click are a documented stopping point.
- The module still implements no non-Settings family; routing is selection
  policy, not implementation authority.

## Revisit when

Revisit when an installed backend changes its `.portal` advertisement, the
keyring provider decision is revised, a first-party backend for a family lands,
KDE ships a wallpaper backend that can target QindaQt's own surfaces, or the
frontend changes interface export so closed families become observable.
