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

## Revisit when

Reconsider the hosting split if measured isolation overhead prevents the panel
latency budget, or a broadly adopted standard can provide equivalent placement,
capability, reliability, and versioning guarantees without loading another
desktop shell runtime.
