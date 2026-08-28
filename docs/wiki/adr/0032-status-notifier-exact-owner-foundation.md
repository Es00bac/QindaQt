# ADR-0032: Key the status-notifier tray on exact unique-name owners

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

- Item ownership is the source's bus **unique name** (e.g. `:1.42`): a
  colon-prefixed sequence of at least two nonempty dot-separated ASCII
  elements using letters, digits, underscore, or hyphen, capped at 255 bytes.
  It is never a well-known name; well-known names are rejected at validation.
- Object paths accept the root path (`/`) or slash-separated valid elements
  up to 255 bytes.
- The registry keys items by `(uniqueName, objectPath)` plus a registry-issued
  owner generation drawn from a globally monotonic counter, so a generation is
  never reissued. When a name departs — with its expected generation named —
  its items are removed and its slot is freed; any event stamped with an older
  or non-current generation or epoch is rejected as stale, so a reply that races
  a disconnect, restart, or watcher transition cannot resurrect removed items.
  Re-basing a still-live name drops that owner's items and reissues a generation,
  so no presented key is ever stale or unactionable.
- A user-visible item identity is claimed by at most one live owner.
  Registering a duplicate identity from a different owner is rejected.
- Every icon, menu, tooltip, and text payload passes bounded, fail-closed
  validation (byte budgets, dimension and count limits, in-place aggregate
  pixmap checking, C0/DEL/C1 and blank control over presentation text, flat
  DBusMenu-style menu entries with depth, submenu-only-parent, and count
  rules) at the single descriptor admission gate before any part of it can
  reach presentation. A rejected current-epoch replacement still observes its
  exact live key for reconciliation while retaining the last-known-good value.
- Activation, context-menu, and secondary-activation requests are validated
  **intents** returned bound to the exact owner generation and item identity
  with an explicit `revalidateIntent` lifetime check before execution; the
  module never performs them.
- Tray presentation is a pure projection with Loading/Ready/Empty/Degraded
  states and keyboard/accessibility identities, so QML and assistive
  technology see stable truth even while the registry is degraded. Watcher
  loss keeps last-known-good items visible and actionable; a watcher
  (re)connection opens an explicit monotonic epoch that returns presentation
  to fail-closed Loading until the population is re-observed and reconciled.
  Epoch exhaustion invalidates the active epoch and remains Loading rather
  than accepting a reused watcher value.
- Transport is an injected interface narrowed to an event sink (owner and
  item events plus watcher epochs, nothing else). `StatusNotifierRegistry`
  and `StatusNotifierEventSink` delete copy and move authority to enforce
  singular ownership. This module contains no D-Bus code; the only allowed
  implementations today are test fakes.

## Consequences

- A later QtDBus adapter milestone adds an exact-owner transport in its own
  module and must report owner departure with the expected generation for
  every name it watches; without that report the registry cannot fence stale
  replies.
- Owner history is bounded: only live owners occupy tracking slots, capacity
  exhaustion fails closed, and generations come from a globally monotonic
  counter so no stale event can ever match a reissued value.
- Watcher epochs also refuse wrap. Empty, partial, and full replacement
  populations reconcile only on a matching completion event; every stale
  completion, owner, item, and mass-removal event is rejected.
- The bounds in `status_notifier_limits.h` are a cross-module contract shared
  by every future producer and consumer; changing one is a contract change.
- Hostile coverage (spoofed owner, stale reply, malformed menu/icon, duplicate
  identity, restart, rebaseline, watcher loss/reconnect, empty/partial/full
  epoch reconciliation, counter exhaustion, and intent revalidation) is part
  of the module's acceptance evidence and must be extended alongside any new
  value type.
- Degraded state retains last-known-good items and requires explicit
  acknowledgement so the tray can surface, then clear, a rejection.
- Presentation text is injected through a localization boundary; the module's
  defaults are deterministic fallbacks, not the product's localized strings.

## Revisit when

A transport milestone needs per-item D-Bus property subscriptions, DBusMenu
revision semantics beyond the bounded flat payload, or persistent tray item
state; any of those would justify extending this record rather than bypassing
it.
