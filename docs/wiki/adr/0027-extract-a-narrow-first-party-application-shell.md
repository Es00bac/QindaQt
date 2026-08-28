# ADR-0027: Extract a narrow first-party application shell

- Status: Accepted
- Date: 2026-08-28
- Deciders: QindaQt first-party application and architecture maintainers
- Technical area: native applications, QML presentation, lifecycle, portals

## Context

The Text Editor established the correct first-party participation shape as an
ordinary independent client. Earlier AppShell analysis intentionally rejected a
broad shared framework before reuse was demonstrated: document models, routes,
sidebars, service policy, platform dialogs and theme selection would have
created coupling without a second consumer.

The next Settings, File Manager and Terminal slices do share a smaller set of
cross-application seams. Each needs application-owned quit consent, stable
menu/action export for local and future global presentation, honest optional
Settings/session readiness, sandbox-compatible file-selection requests,
bounded failures, deterministic initial focus, and the same QST/Controls window
surface. Copying those ownership and fencing rules into each app would make
them diverge at precisely the cross-process and data-loss boundaries where
consistency matters.

## Decision

We will ship `QindaQt.AppShell 1.0` as one narrow public C++/QML module.

- An application-owned GUI-thread coordinator projects bounded state and emits
  requests; it never exits the process or performs domain actions.
- The action registry atomically exports stable, ordered menu/action values and
  emits activation intent. It owns neither action execution nor global
  shortcuts.
- Settings/session readiness is injected as confirmed `NotRequired`, `Ready`,
  `Degraded`, or `Unavailable` state. The module has no service client.
- File/folder requests and results use typed, serialized, request-ID-fenced
  values. The module has no portal, dialog, bus, permission, or grant adapter.
- The reusable `ApplicationWindow` consumes only public QST-1, Controls 1.0,
  and the coordinator. It owns menu/degraded/focus presentation, not theme
  selection, routes, navigation, app content, or desktop identity.
- First-party apps remain supervised non-essential ordinary clients. AppShell
  introduces no daemon, singleton process, shell lookup, compositor API, or
  session-supervisor dependency.

The public version-1 compatibility surface comprises the type names, enum and
error meanings, bounds, request/response ownership, action snapshot keys, QML
properties/signals, and close/focus behavior documented in
[QindaQt.AppShell 1.0](../apps/application-shell.md).

## Consequences

- Upcoming apps share the failure-prone participation seams without sharing
  their domain models or navigation.
- An application must explicitly resolve quit and portal requests. A missing
  owner fails closed rather than silently losing data or falling back to an
  in-process dialog.
- The menu snapshot can feed future global-menu work without creating a shell
  dependency today. Export transport and external registrar policy need their
  own boundary and ADR.
- Apps must continue setting desktop identity and publishing complete QST-1
  values before constructing the surface.
- A real portal adapter, Settings/session hook adapter, concrete app migration,
  live AT bridge, nested capture, and global menu remain separately qualified
  outcomes.
- Adding routes, persistence, process lifetime, service discovery, platform
  handles, shell policy, or app-domain values to this module requires a new or
  superseding ADR.

## Alternatives considered

### Continue copying every seam per application

Rejected for the now-demonstrated cross-app lifecycle, action export, portal
fencing, and accessibility shell behavior. Domain and route code still remain
per-app.

### Adopt a complete application framework

Rejected. A route registry, sidebar templates, service locator, platform
dialogs, persistence and process control would reverse established module
dependencies and make simple clients pay for unrelated policy.

### Put these seams in QindaQt.Controls

Rejected. Controls are token-styled presentation primitives without
application lifecycle or request authority. AppShell consumes Controls; it
does not widen Controls into an application coordinator.
