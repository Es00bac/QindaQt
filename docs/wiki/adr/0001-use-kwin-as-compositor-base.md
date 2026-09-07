# ADR-0001: Use KWin as the compositor base

- **Status:** Accepted
- **Date:** 2026-08-25
- **Owners:** Compositor
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt needs production-grade Wayland protocol handling, rendering, input,
XWayland, output management, color management, and hardware support. Building
those foundations from scratch would dominate development time and create avoidable
security, compatibility, and driver risk. The product-specific work is the
hybrid [window-container model](../architecture/window-containers.md) and its
shell experience.

## Decision

Base `qindaqt-wm` on the KWin 6.6 line and track stable upstream releases. Keep
QindaQt behavior in a small, reviewable downstream integration layer and
isolated patches. Use supported KWin extension points where possible; do not
couple the shell to KWin private objects. The native session is Wayland, with
rootless XWayland started on demand and no separate native X11 session.

## Current operational pin

The release workspace pins KWin `6.6.6` exactly: tag object
`43cb730ca363b995dfd5f0ceb537e4c37a7bb5ff`, commit
`9bf2235fad10de9048c634e376bf12e56b3023e6`, and tree
`88f96f8cde49c51552d82f60fd461b6e8b950685`. The current downstream patch
series is empty. The binary plugin is compiled for
`org.kde.kwin.PluginFactoryInterface6.6.6`; it must be rebuilt and the
integration matrix rerun for every KWin patch release rather than assuming ABI
compatibility. Pin verification and plugin discovery are detailed in
[Compositor and session integration](../architecture/compositor-session.md).

## Consequences

- QindaQt inherits mature protocol, graphics, and multi-output behavior and must
  comply with upstream licensing.
- The compositor team maintains a documented rebase workflow, an upstream patch
  inventory, and integration tests against supported KWin versions.
- QindaQt-specific state crosses the boundary through versioned interfaces;
  shell and service code remain independently testable.
- Upgrades are test-gated and may require adapting isolated patches, but must not
  silently alter container invariants or public protocols.
- An exact release match is required at configure time for the runtime plugin.
  The default plugin-enabled configuration fails rather than silently omitting
  the compositor service; a build without matching KWin development files must
  explicitly select the bridge-only option.

## Revisit when

Reconsider if KWin can no longer meet the performance budget, required behavior
cannot be maintained without a broad invasive fork, or another mature base
demonstrably reduces total maintenance and compatibility risk.
