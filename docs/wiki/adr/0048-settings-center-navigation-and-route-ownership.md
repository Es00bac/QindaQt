# ADR-0048: Keep Settings navigation typed and route authority local

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** First-party Settings Center
- **Supersedes:** None
- **Superseded by:** None

## Context

Notifications and Appearance were independently executable Settings pages,
but the application selected one during startup and had no shared navigation
model. A real Settings Center must switch between pages responsively without
turning the application into a service locator, duplicating route authority in
QML, constructing two pages during a layout transition, or letting one
Settings1 client consume another client's reply. It must also reject hostile
startup route intent and preserve the QST-1 publish-before-construction rule.

The owning behavior is documented in the
[Settings Center](../apps/settings-center.md). Appearance remains governed by
[ADR-0028](0028-compose-appearance-settings-through-settings1.md), and the
navigation shell does not broaden
[QindaQt.AppShell](../apps/application-shell.md) into a route framework.

## Decision

1. Settings owns a small process-local navigation library containing bounded
   route descriptors, a deterministic registry, and one selection controller.
   IDs are external intent; a closed enum selects a compiled route component.
   Descriptors never contain a QML URL, implementation object, transport, or
   persistence handle.
2. The startup command accepts only registered built-in IDs and rejects every
   other value before constructing transport, models, or QML. Runtime unknown
   selection preserves the current route. Invalid descriptors and contradictory
   availability diagnostics do not enter the registry.
3. Wide and compact presentations share the controller but have separate
   hosts. Exactly one host is presentation-active, and exactly one Loader in
   that host constructs a page. Unknown component values or registered
   unavailable routes render an explicit fail-closed notice.
4. Every domain route retains its own model and scoped client. Notifications
   and Appearance use independent Settings1 transports because client request
   tokens are local and may collide when fanned into one transport. Domain
   objects live for the application process so navigation does not discard
   confirmed state.
5. The navigation shell consumes only QST-1 and QindaQt.Controls, publishes a
   complete token generation before root construction, and owns PageTabList,
   PageTab, responsive layout, shortcut, focus-entry, and focus-return
   semantics. A domain page owns the controls and focus cycle within its page.

## Consequences

- Adding a route requires a coordinated C++ descriptor/component-enum change,
  a compiled host mapping, one domain composition, documentation, and focused
  plus installed-package evidence. Arbitrary runtime QML route plugins are not
  supported by S1.
- Both current route models and two lightweight D-Bus transport objects live in
  one Settings process. Only one route page is instantiated, so navigation
  retains state without duplicating visual trees or page-side focus effects.
- A missing theme catalog or Tokens module is fatal to the complete Settings
  shell, including Notifications, because navigation itself is token-styled.
  Settings1 loss is not fatal: each domain model exposes its existing truthful
  degraded/retry behavior.
- Offscreen, sanitized installed-package, hostile-intent, keyboard/focus, and
  accessibility tests are mandatory. Live AT-SPI and nested desktop matrices
  remain separate qualification work.

## Revisit when

Reconsider the closed component registry when a signed third-party settings
extension protocol has explicit process isolation, capability policy,
localization, lifecycle, and hostile-package tests. Reconsider per-route
process lifetime only after measurements show that keeping bounded domain
models alive materially violates the desktop resource budget.
