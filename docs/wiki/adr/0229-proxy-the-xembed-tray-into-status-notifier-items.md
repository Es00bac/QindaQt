# ADR-0229: proxy the legacy XEmbed tray into StatusNotifier items

- **Status:** Accepted
- **Date:** 2026-09-21
- **Owners:** Platform services (tray interop)
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt's tray presents StatusNotifierItem (SNI) clients over D-Bus and, since
[ADR-0166](0166-announce-a-status-notifier-host.md), announces a specification
host so conformant items appear. That ADR left one half of the user-visible
problem open in writing: applications that speak **only** the legacy XEmbed
system tray protocol — most Wine/Proton programs and older toolkits — still
have nowhere to dock, because nothing in the session owns the
`_NET_SYSTEM_TRAY_S0` selection on the XWayland display. A Windows
application's tray icon simply never appears.

The tray applet itself is presentation over a bounded facade and must not grow
X11 authority, so the gap is owned by a platform service, not the shell. The
governing boundaries are
[Module boundaries](../architecture/module-boundaries.md) and the tray page
[Status notifier tray](../shell/status-tray.md). The reference point for the
protocol mechanics is KDE's `xembed-sni-proxy` (plasma-workspace), whose
fifteen years of toolkit workarounds this decision deliberately reuses while
changing its failure policy: KDE's proxy exits when the selection is already
owned or lost; this proxy must not. Two trays fighting over one selection is
worse than one missing icon.

## Decision

A new resident session service, `qindaqt-xembed-tray-proxy`
(`src/services/xembed_tray_proxy`), owns the XEmbed system-tray selection and
republishes every docked client as a StatusNotifierItem, so the existing tray
applet presents Wine/Proton icons with **no applet change**.

- **Selection ownership.** The service connects to the session's X display
  (XWayland), creates a small never-shown owner window, and acquires
  `_NET_SYSTEM_TRAY_S<screen>`. It sets `_NET_SYSTEM_TRAY_VISUAL` honestly
  (the 32-bit TrueColor visual when one exists, else the root visual) and
  broadcasts the `MANAGER` client message after claiming. If the selection is
  already owned, the service logs the owning window and stays **inert**: it
  subscribes to XFixes selection notifications and claims the selection only
  after the current owner releases it. If ownership is later lost
  (`SelectionClear`), the service retires every published item, reparents
  embedded clients back to the root window so they can dock with the
  successor, and returns to the inert state. It never exits over selection
  contention.
- **Embedding.** A docking client (`SYSTEM_TRAY_REQUEST_DOCK` on the owner
  window) is embedded per XEmbed 0.1: a per-icon override-redirect container
  window using the tray visual, `_NET_WM_WINDOW_OPACITY` 0 and an empty input
  shape so the container is never visible and never steals clicks; the client
  window is reparented in, composite-redirected manually, mapped, sent
  `XEMBED_EMBEDDED_NOTIFY`, and inserted into the save-set so proxy death
  reparents clients to the root window instead of destroying them.
  `SYSTEM_TRAY_BEGIN_MESSAGE`/`CANCEL_MESSAGE` balloons are accepted and
  ignored; SNI has no balloon concept and inventing one is not this service's
  contract.
- **The picture.** An XEmbed tray icon is a live X window, not an image.
  Because the client window is composite-redirected, its contents survive
  offscreen, so the proxy reads pixels with `xcb_get_image` directly on the
  client window and converts the ZPixmap to premultiplied ARGB32 — exactly
  the `Pixmap.argb` wire layout the shell's icon renderer validates against
  (`status_notifier_limits.h`: at most 8 pixmaps, 512 px edges, 1 MiB). The
  name-window-pixmap alternative (XComposite pixmap + `XGetImage`) was
  rejected: it adds a server-side pixmap per icon and a second failure mode
  for zero gain over reading the redirected window, the shape KDE converged
  on after shipping both. A captured frame that is **fully transparent** is
  dropped in favour of the previous frame; Wine is known to emit transparent
  frames transiently. Client repaint is driven by XDamage watches, coalesced
  to at most one capture per 33 ms per icon, plus one hedged re-capture after
  embedding because no damage event covers the first paint. Depth-24 captures
  are forced opaque; dimensions above 128 px are clamped at capture.
- **Publication.** Each docked icon becomes one SNI item: `Id`/`Title` from
  `_NET_WM_NAME` (bounded; fallback `xembed-<window>`), `Category`
  `ApplicationStatus`, `Status` `Active`, `ItemIsMenu` `false`, `Menu`
  `/NO_DBUSMENU` — the shape the shell already proves with its pixmap-only
  Wine regression row. Icon changes raise `NewIcon`. **One private D-Bus
  connection per item**: the owning unique name is what the watcher tracks,
  so retirement is the connection closing — the only truthful removal the
  protocol and the shell's watcher both guarantee. Items additionally claim
  the conventional `org.kde.StatusNotifierItem-<pid>-<n>` name when free.
- **Interaction.** `Activate`/`SecondaryActivate`/`ContextMenu` become
  synthetic button 1/2/3 press-release pairs on the client window, and
  `Scroll` becomes button 4–7 per sign and orientation. Before dispatch, the
  container is moved so the icon sits under the panel-supplied global
  coordinates and the X pointer is warped into the client window, because GTK
  and Wine both sanity-check event coordinates against the window's position.
  Delivery is a direct `xcb_send_event` when the client (or a descendant)
  selected button-press events, else XTest; XTest delivery is asynchronous
  under XWayland's libei path, so the input shape stays active briefly before
  being withdrawn. A Wine tray menu is an X11 popup the client owns:
  `ContextMenu` forwards the click and the client places its own menu; no
  dbusmenu translation is attempted.
- **Lifetime.** `DestroyNotify`, `UnmapNotify`, or the client reparenting out
  retires the item. Embedded resize `ConfigureRequest`s resize the container
  (clamped) instead of being honoured blindly. Docking is bounded at 64
  icons, matching the shell registry ceiling; further docks are refused and
  logged. Nothing blocks the shell: the proxy is a separate process, every
  D-Bus call is asynchronous, and X round-trips never cross into the shell
  process.

## Consequences

- Wine/Proton and other XEmbed-only applications gain a working tray icon on
  a QindaQt session with zero shell or applet changes; the request-checklist
  row moves from `gap` to `committed`.
- The module links `libxcb`, `xcb-composite`, `xcb-damage`, `xcb-shape`,
  `xcb-xtest`, and `xcb-xfixes` — new dependencies confined to
  `src/services/xembed_tray_proxy` by its boundary test; the shell, applet,
  and compositor gain none.
- Operationally the service needs an X display: it starts with the graphical
  session, waits briefly for XWayland, and on a pure-Wayland display-less
  session exits cleanly rather than spinning.
- Click fidelity inherits the protocol's limits: toolkits that reject
  synthetic events entirely are out of scope, and shaped-icon hit refinement
  uses the window centre, not KDE's bounding-rectangle scan.
- Proof without Wine comes from a minimal xcb fixture client under a nested
  XWayland server and a private D-Bus daemon: dock, a non-empty `IconPixmap`
  read over the bus, forwarded clicks arriving at the fixture, damage-driven
  icon refresh, retirement on window destruction, and a second proxy staying
  inert until the first releases the selection.

## Revisit when

- The applet genuinely cannot present a proxied item — that is a shell
  change and the manager's call, not this service's.
- A client needs balloon messages, shaped-click refinement, or
  `_NET_WM_ICON` fallback icon reads; each is an additive change behind the
  same backend seam.
- The session gains a second X display or per-seat trays, at which point the
  single `_NET_SYSTEM_TRAY_S<screen>` ownership model needs a policy.
