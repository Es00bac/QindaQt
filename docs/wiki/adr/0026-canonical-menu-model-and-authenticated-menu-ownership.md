# ADR-0026: Canonical menu model with authenticated active-window ownership

- **Status:** Proposed
- **Date:** 2026-08-28
- **Owners:** Shell / global menu
- **Supersedes:** None
- **Superseded by:** None

## Context

The panel's global-menu applet must show the focused application's menu without
turning the shell into an application framework or trusting every process on
the session bus. The de facto standard, `com.canonical.AppMenu.Registrar`,
lets any bus client register a menu for any window id it names; the registrar
does not authenticate that the registering peer owns the window or even that
the window exists. Reproducing that trust model would let a hostile client
replace or spoof another application's menu and redirect its shortcuts.
First-party menus also need to survive toolkit diversity eventually, while
today only native Qt menus exist (the Text Editor's ordinary `QMenuBar`/
`QAction` tree per [ADR-0022](0022-keep-text-documents-local-and-atomic.md)).
The owning behavior and milestone boundary are in
[Global application menu](../shell/global-menu.md).

## Decision

The shell owns a bounded, toolkit-neutral canonical menu/action model and an
authenticated ownership policy:

- Every menu crossing a boundary is a canonical `MenuTree` value with fixed
  hostile-input ceilings, toolkit-neutral mnemonic representation, globally
  unique stable ids, and owner/epoch/revision lineage mirroring Display1,
  Audio1, and Settings1. Validation rejects malformed input as a whole.
- A provider registration is accepted only when a compositor-authenticated
  active-window source names that exact window and a bus-daemon credential
  lookup proves the registering peer's OS process owns that window. G0
  authenticates only the currently active window; per-window registration
  caches and foreign-application injection are out of scope until separately
  reviewed.
- The exporter assigns lineage itself, validates every pulled snapshot, and
  fails closed by retaining the last accepted tree on any invalid pull.
- Invocations are authorized against the current lineage (window, epoch, and
  presented tree must agree) before any action lookup; stale, unknown,
  non-action, and disabled targets are rejected without side effects.
- Toolkit adaptation is confined to adapters; the Qt Widgets `QMenuBar`
  adapter is the only component permitted to link `Qt6::Widgets` inside the
  global-menu module, so the QtQuick shell stays Widgets-free.

## Consequences

- A hostile or buggy client cannot publish a menu for a window it does not
  own, and a stale UI request cannot trigger an action of a provider that has
  since lost ownership.
- Compatibility with `com.canonical.AppMenu.Registrar` is deliberately not
  provided in this milestone; toolkits or apps needing it require a future
  ADR covering the trust gap.
- Applications that want stable deltas must publish persistent action object
  names; positional fallback ids degrade gracefully but are only stable while
  the sibling structure is unchanged.
- The model's fixed ceilings reject pathologically large menus wholesale; a
  legitimate application exceeding them must be redesigned or the limits
  widened through the same governance as other wire-limit changes.
- Focused hostile tests must cover validation bounds, authentication
  rejection paths, fail-closed export, deterministic delta ordering, stale
  invocation rejection, and facade activation gating before any transport
  milestone.

## Revisit when

A real cross-toolkit or legacy application must export menus, a per-window
registration cache is product-required, or a compositor-authenticated window
inventory becomes available to replace the injected G0 seams with a resident
transport.
