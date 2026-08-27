# ADR-0002: Define a native QindaQt applet API

- **Status:** Accepted
- **Date:** 2026-08-25
- **Owners:** Shell and SDK
- **Supersedes:** None
- **Superseded by:** None

## Context

Panels and docks must be composable enough to express classic taskbars, global
menu bars, launch docks, vertical rails, multi-row lists, monitors, and custom
arrangements. Loading Plasma and its widget runtime would undermine QindaQt's
resource and interaction goals, while an unbounded in-process extension API
would make shell reliability and security depend on every third-party applet.

## Decision

Define `QindaQt.Applets 1.0` as QindaQt's native manifest and runtime API. Each
manifest declares API version, entry point, supported placements, sizing,
settings schema, and requested capabilities. Audited built-in applets may run in
the shell process. Third-party applets run in capability-grouped sandboxed hosts;
the shell positions their surfaces and mediates privileged resources through
versioned interfaces.

The API is independent of [layout profiles](../shell/layout-profiles.md).
Profiles instantiate and arrange compatible applets but cannot grant additional
capabilities or execute code. Plasma widget compatibility is not a goal.

## Consequences

- Applet crashes can be contained and hosts restarted without terminating the
  shell.
- Capability mediation, manifest validation, API negotiation, malformed-input,
  and crash/restart tests are required before third-party applets are enabled.
- Private shell objects are never part of the extension contract; API evolution
  occurs through explicit versions and compatibility documentation.
- Cross-process surfaces and frequent data streams need performance budgets so
  isolation does not make panels sluggish.

## Implementation status

Schema v1 parsing, normalization, API negotiation, validation, catalog loading,
host selection, and capability-policy evaluation are implemented. The launcher,
task-list, global-menu, status-tray, clock, and notification-center manifests
are data contracts; a manifest alone is never evidence of executable UI.
Production resolution also requires placement compatibility, accepted host
mode, and an exact entry in a compiled audited-built-in registry before exposing
any capability grants.

The locale-aware clock and dedicated notification-center entry currently
satisfy that last gate and render as live production QML. The notification
entry requests no capabilities and receives only a shell-private toggle/open-
state facade with read-only Do Not Disturb status, not policy mutation,
notification records, or service operations. Other profile
entries are retained with an explicit `missing-manifest` or
`implementation-unavailable` status and visible warning marker instead of being
reported as working. Sandboxed hosts and platform-service mediation remain
later runtime slices. See the
[manifest reference](../reference/applet-manifest-schema-v1.md) and
[applet runtime](../shell/applet-runtime.md).

## Revisit when

Reconsider the hosting split if measured isolation overhead prevents the panel
latency budget, or a broadly adopted standard can provide equivalent placement,
capability, reliability, and versioning guarantees without loading another
desktop shell runtime.
