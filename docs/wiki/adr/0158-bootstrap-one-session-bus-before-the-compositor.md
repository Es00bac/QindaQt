# ADR-0158: Bootstrap one session bus before the compositor

- **Status:** Accepted
- **Date:** 2026-09-13
- **Owners:** Session, Compositor, Platform services
- **Supersedes:** ADR-0134's claim that KGlobalAccel runs inside KWin
- **Superseded by:** None

## Context

The display-manager session may start `qindaqt-wm` without
`DBUS_SESSION_BUS_ADDRESS`. In that environment KWin starts with no user bus,
then a later Qt process may autolaunch a private broker for itself and its
children. That divides one desktop into two authority graphs: the shell can see
its activated services, but cannot see KWin's `org.kde.KWin` or QindaQt
compositor services. Output truth, pointer devices, task-list facts,
customization adoption, and global shortcuts consequently fail together.

The Plasma 6 KGlobalAccel authority is also a separate `kglobalacceld` process,
not code running inside KWin as ADR-0134 stated. A private QindaQt bus cannot
depend on the distribution's user-systemd unit being attached to that broker.

## Decision

Before inspecting or replacing itself with KWin, `qindaqt-wm` checks the
inherited session-bus address. If it is absent, the launcher replaces itself
with `dbus-run-session --` followed by its exact original argument vector. The
inner launcher, KWin, the KWin plugin, `qindaqt-session`, the shell, and every
D-Bus-activated service then inherit one broker. An explicitly supplied bus is
never replaced.

`qindaqt-session` owns the installed `kglobalacceld` as an optional,
parent-lifetime-bound child on that same broker. Missing optional binaries do
not prevent login; private tests can disable the child explicitly. Settings and
shell clients continue to consume the public `org.kde.kglobalaccel` API and do
not gain process-management responsibility.

## Consequences

- Compositor output, input, task, and layout-adoption services share the exact
  bus used by shell and Settings consumers.
- The bus wrapper owns broker cleanup and returns the compositor session's exit
  status; no detached broker remains after logout.
- A changed bus topology takes effect at the next login. Replacing binaries in
  a running split-bus session cannot reconnect KWin retrospectively.
- Unit tests pin bootstrap/no-bootstrap argument behavior and optional shortcut
  daemon lifetime. Nested production qualification must assert that KWin's
  D-Bus API, the `org.qindaqt.Compositor` bus name, and
  `org.kde.kglobalaccel` are present on the same broker observed by the shell.

## Revisit when

Every supported display manager supplies a verified user-session bus before
the Wayland session entry, or QindaQt adopts a session manager that creates and
owns the broker through an equivalent explicit lifecycle contract.
