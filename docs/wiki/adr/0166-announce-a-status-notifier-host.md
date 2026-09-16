# ADR-0166: announce a StatusNotifier host, not only a watcher

- **Status:** Accepted
- **Date:** 2026-09-15
- **Owners:** Shell (status tray)
- **Supersedes:** None
- **Superseded by:** None

## Context

The StatusNotifierItem specification separates two roles. The **watcher**
(`org.kde.StatusNotifierWatcher`) is a registry: items register with it and
hosts register with it. A **host** is the thing that actually draws the items.
An item is expected to consult the watcher's
`IsStatusNotifierHostRegistered` property, and to follow the
`StatusNotifierHostRegistered` signal, before deciding whether presenting
itself over this protocol is worthwhile at all. Items that find no host
legitimately hide their icon, decline to register, or fall back to the legacy
XEmbed system tray.

QindaQt served the watcher from the shell
(`src/shell/runtime/statusnotifierappletcomposition.cpp` starting
`StatusNotifierWatcherService`) but nothing ever called
`RegisterStatusNotifierHost`. The watcher implemented the method and the
property; the shell simply never used them on itself.

Measured on the user's live session on 2026-09-15 (shell pid 129167):

    RegisteredStatusNotifierItems  -> [":1.44/StatusNotifierItem"]
    IsStatusNotifierHostRegistered -> false

`:1.44` was a real Wine item (Battle.net, `Id "wine-0x100de-0"`,
`Status "Active"`) supplying a valid 16x16 ARGB `IconPixmap` and an empty
`IconName`. So the tray pipeline had a live, well-formed item available and the
session still presented as "the system tray does not work".

A regression row added with this change
(`projectsAPixmapOnlyItemWithNoExportedMenu`) reproduces that exact item shape
— empty `IconName`, pixmap-only, `Menu` of `/NO_DBUSMENU`, no overlay, no
tooltip — and passes both before and after, which establishes that projection,
validation and icon rendering were never the defect. The missing host
announcement was.

## Decision

The shell announces itself as a specification host.

- A new focused component,
  `QindaQt::StatusNotifier::StatusNotifierHostRegistration`
  (`src/shell/status_notifier/host/`), owns the conventional per-process bus
  name `org.kde.StatusNotifierHost-<pid>` and calls
  `RegisterStatusNotifierHost` with it.
- The registration goes **over the bus**, not through a direct in-process call
  into our own watcher service. The observable protocol traffic is then
  identical to any other conformant host, and the same code works unchanged
  when a different implementation owns the watcher name. The call is
  asynchronous by necessity: in the production shell the callee lives in the
  same process on the same connection, and a blocking call would wait on a
  reply only this thread's event loop can produce.
- Startup order is not assumed. A watcher that is not on the bus yet is not a
  failure; the component watches `NameOwnerChanged` (filtered to the bus daemon
  as sender) for the watcher name and completes the handshake when a watcher
  appears, re-registers against a replacement watcher, and drops its claim
  when the watcher goes away, since a fresh watcher starts with an empty host
  set.
- The announcement is gated on the same `status-items.read` capability that
  lets the adapter project items. A shell that cannot draw items must not tell
  every item that a tray exists.
- This is a new component rather than a method on an existing one because the
  monitor adapter's contract explicitly forbids opening connections or calling
  QDBus APIs, and the watcher service is the protocol's *server* role; folding
  the host role into either would blur a distinction the specification draws.

## Consequences

- Conformant items that gate on host presence now present themselves on a
  QindaQt session. `IsStatusNotifierHostRegistered` is true whenever the tray
  applet can observe.
- QindaQt owns a second well-known bus name per shell process. It is released
  on shutdown and is the name every other desktop uses for this role.
- This fixes only the SNI half of the user-visible tray problem. Applications
  that speak **only** the legacy XEmbed tray protocol still have nowhere to
  dock, because QindaQt owns no `_NET_SYSTEM_TRAY_S0` selection. That is a
  separate outcome, tracked in the task list, and it is what most Wine/Proton
  programs and older toolkits use.

## Revisit when

- An XEmbed-to-SNI proxy lands: it will publish items as a separate bus client
  and must not be confused with this host role.
- More than one QindaQt shell process can run against a single session bus, at
  which point the pid-derived name stops being unique per *session* and the
  host set needs a policy.
