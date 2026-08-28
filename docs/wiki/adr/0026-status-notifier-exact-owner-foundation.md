# ADR-0026: Key the status-notifier tray on exact unique-name owners

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Shell status-notifier module
- **Supersedes:** None
- **Superseded by:** None

## Context

StatusNotifier tray items arrive from third-party session services. A source
can disconnect at any moment, its well-known bus name can be claimed by
another process while items remain registered, and its payloads (icons, menus,
tooltips) are untrusted input that reaches user presentation and assistive
technology. A naive tray model keyed on well-known names would let one process
impersonate another's items, and unbounded payloads would let one source
exhaust shell memory or bury the accessibility tree.

The tray's S0 milestone is source/static only: no live session bus, no watcher
ownership, and no action execution. The value model and ownership policy must
exist first so a later transport milestone can only add a narrow adapter, not
rediscover hostile-input handling.

## Decision

The shell's `status_notifier` module owns a pure, Qt Core-only item model whose
trusting rules are:

- Item ownership is the source's bus **unique name**, never a well-known name.
  Well-known names are rejected as owners at validation.
- The registry keys items by `(uniqueName, objectPath)` plus a registry-issued
  owner generation. When a name departs, its items are removed and the
  generation is retained only to fence: any event stamped with an older or
  non-current generation is rejected as stale, so a reply that races a
  disconnect or restart cannot resurrect removed items.
- A user-visible item identity is claimed by at most one live owner.
  Registering a duplicate identity from a different owner is rejected.
- Every icon, menu, tooltip, and text payload passes bounded, fail-closed
  validation (byte budgets, dimension and count limits, control-character
  rejection, flat DBusMenu-style menu entries with depth and cycle rules)
  before it can reach presentation.
- Activation, context-menu, and secondary-activation requests are validated
  **intents** against exact live ownership; the module never performs them.
- Tray presentation is a pure projection with Loading/Ready/Empty/Degraded
  states and keyboard/accessibility identities, so QML and assistive
  technology see stable truth even while the registry is degraded.
- Transport is an injected interface. This module contains no D-Bus code; the
  only allowed implementations today are test fakes.

## Consequences

- A later QtDBus adapter milestone adds an exact-owner transport in its own
  module and must report owner departure for every name it watches; without
  that report the registry cannot fence stale replies.
- The bounds in `status_notifier_limits.h` are a cross-module contract shared
  by every future producer and consumer; changing one is a contract change.
- Hostile coverage (spoofed owner, stale reply, malformed menu/icon, duplicate
  identity, restart) is part of the module's acceptance evidence and must be
  extended alongside any new value type.
- Degraded state retains last-known-good items and requires explicit
  acknowledgement so the tray can surface, then clear, a rejection.

## Revisit when

A transport milestone needs per-item D-Bus property subscriptions, DBusMenu
revision semantics beyond the bounded flat payload, or persistent tray item
state; any of those would justify extending this record rather than bypassing
it.
