# ADR-0101: Launch workspace applications through desktop entries

- Status: Accepted
- Date: 2026-09-07

## Context

A saved workspace contains desktop-entry identifiers and URLs. Reopening must
honor desktop-file argument expansion, application startup behavior and Wayland
activation. A hand-written command parser would duplicate desktop platform
behavior and make legitimate application launches unreliable.

## Decision

Add a separate `QindaQt::WorkspacesApps` desktop adapter using KF6 Service for
application lookup and KF6 KIOGui's `ApplicationLauncherJob` for asynchronous
launch. The pure Workspaces model remains dependent only on Core and Qt Core.
Build this adapter for the compositor and its tests, not for apps-only builds.
The full desktop package must declare KService and KIO dependencies.

The caller supplies any Wayland activation token. Launch returns whether the
request was dispatched; its completion signal reports process-start success or
failure. Neither is a claim that the compositor has received an eligible
window. Reopen must refresh its window inventory independently and surface
launch failures. Missing desktop entries and invalid URLs reject immediately.

## Consequences

Desktop-entry execution follows the installed KDE platform implementation.
Application lookup accepts both a storage ID and the common application ID
without its `.desktop` suffix. The QObject adapter is owned by its caller and
used only on its GUI thread. Jobs are asynchronous and never block the UI with
`exec()` or wait for application exit.

See [Saved workspaces](../architecture/workspaces.md).
