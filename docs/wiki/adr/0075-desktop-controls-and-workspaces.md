# ADR-0075: Compose desktop controls over bounded shell facades

- **Status:** Accepted
- **Date:** 2026-09-05
- **Owners:** Shell workgroup
- **Supersedes:** None
- **Superseded by:** None

## Context

Stock layout profiles need usable workspace, launcher, status, menu, and
application controls while the shell remains independent of compositor and
service implementation objects. The panel also needs one deterministic loader
path and keyboard-capable popup surfaces.

## Decision

Implement the thirteen desktop control entry points as a compiled
`QindaQt.Shell.DesktopControls 1.0` module over bounded, shell-owned facades.
The runtime resolves each manifest instance through the existing audited
registry and capability policy, then passes one selected facade to the panel
loader. Workspace state is obtained through the public KWin workspace adapter,
with exact-owner and owner-change fencing; an optional supervisor compositor
PID is preferred when supplied and the trusted compositor peer is retained as
the ordinary fallback. Completed workspace operations trigger a fresh snapshot
read to converge when a compositor signal is absent.

Application tiles and Quick Launch share launcher authority but have distinct
presentations. Application tiles activate only through the launcher's bounded
intent path. Dashboard receives a small view composed from status, workspace,
and launcher facades.

## Consequences

The stock profiles gain real compiled controls without importing KWin, service
clients, or platform objects into QML. Facade ownership and one-loader
composition remain explicit, and stale workspace actions cannot silently apply
to a replacement owner. The runtime performs an extra workspace read after
each completed operation, and application tiles initially expose the bounded
pinned application set rather than a second application catalog.

## Revisit when

Revisit when a supervisor-provisioned compositor identity becomes mandatory,
when launcher policy needs a separate all-applications tile catalog, or when
installed nested-shell interaction provides evidence beyond the focused
offscreen and injected-transport rows.
