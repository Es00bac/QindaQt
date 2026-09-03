# ADR-0068: Compose first-party menu export through AppShell

- **Status:** Accepted
- **Date:** 2026-09-03
- **Owners:** AppShell, first-party applications
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0027](0027-extract-a-narrow-first-party-application-shell.md) keeps the
first-party AppShell coordinator free of platform/service authority while
making its deterministic action snapshot the shared local menu contract.
[ADR-0033](0033-canonical-menu-model-and-authenticated-menu-ownership.md) and
[ADR-0056](0056-adopt-standard-appmenu-dbusmenu-transports.md) define the
canonical model and accepted standard transports. [ADR-0063](0063-project-authenticated-active-window-identity.md)
requires the production shell to verify the exporter PID against either the
focused XWayland AppMenu window id or the native-Wayland announced menu
address. First-party applications need one reusable composition boundary that
connects those contracts without putting D-Bus or platform discovery into the
coordinator or duplicating shell authentication.

## Decision

- Add an opt-in `QindaQt::AppShell::MenuExport` module beside, rather than
  inside, the transport-free AppShell core. It borrows an
  `ApplicationCoordinator`, primary `QWindow`, and injected session-bus
  connection and owns its identity publisher and export lifetime.
- Project the coordinator's atomic deterministic action snapshot into
  lineage-free canonical content and serve it at
  `/org/qindaqt/AppShell/Menu` through the complete standard server owned by
  `QindaQt::GlobalMenuDbusMenu`. The application never invokes
  `MenuExporter`, issues an owner/epoch/revision, or otherwise competes with
  the shell selector: only the authenticated shell composition stamps
  accepted lineage after decoding the wire snapshot.
- Route one admitted dbusmenu `clicked` event through
  `ApplicationCoordinator::activateAction()` exactly once. Its current
  known/enabled gate remains the application consent authority shared with the
  in-window menu. Transport uncertainty is never retried as an action.
- Use only asynchronous registrar/name calls. Track the exact current owner of
  `com.canonical.AppMenu.Registrar`; withdraw and republish on owner
  loss/replacement. A close event is only a request until the application has
  handled it; withdraw after an accepted close makes the surface non-visible,
  or on composition/window destruction, but retain the association when
  AppShell rejects close for application consent.
- On `xcb`, register the real positive 32-bit `QWindow::winId()`. On native
  Wayland, publish no numeric id. Confine Qt 6.11's private Unix platform
  `registerDBusMenuForWindow` hook to one adapter, after a native Wayland
  surface exists, so Qt announces the application's unique bus name and object
  path through the KDE appmenu protocol consumed by ADR-0063. The build is
  intentionally tied to the exact Qt private-header version and must be rebuilt
  with Qt upgrades.
- Publication/registration means only that an endpoint is available. It does
  not prove that the authenticated shell hosts the menu. First-party local menu
  bars therefore remain visible and authoritative.

File Manager is the first consumer and retains one exporter composition beside
its coordinator/window. Text Editor and Terminal can use the same opt-in line;
they do not import File Manager or shell runtime implementation.

## Consequences

- AppShell core keeps its narrow ADR-0027 boundary, while the optional adapter
  has explicit DBus/Gui/global-menu dependencies and constructor-visible
  authority.
- XWayland and native Wayland publish only platform-true identity facts. The
  application does not claim these are authenticated; G2 still performs the
  focused-window, unique-owner, and PID proof before display or invocation.
- A missing bus, registrar, native surface, unsupported platform, malformed
  snapshot, or registration failure leaves export disabled/waiting without
  affecting local menus or application use.
- One transport-owned dbusmenu v4 server implements `GetLayout`,
  `GetGroupProperties`, `GetProperty`, `Event`, `EventGroup`, `AboutToShow`,
  and `AboutToShowGroup`, including depth/property filters, the standard
  empty-ID request for all published non-root item properties, and atomic
  malformed snapshot rejection. AppShell supplies content and consumes
  admitted action IDs; it owns no second protocol object.
- Tests require real dbusmenu traffic on a private bus, the complete v4
  surface, owner transition, accepted/rejected close cases, exactly-once and
  disabled activation controls, lineage/source-boundary poison, and an actual
  first-party process consumed by production shell composition under matching,
  PID-mismatched, and window-ID-mismatched identity snapshots.

## Revisit when

Qt exposes a public native-Wayland appmenu registration API, the shell adds an
authenticated hosted-state protocol that can safely control local menu
visibility, or a delegated exporter must run in a PID different from the
application window owner.
