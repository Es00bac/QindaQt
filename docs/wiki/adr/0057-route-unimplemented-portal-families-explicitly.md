# ADR-0057: Route unimplemented portal families explicitly

- **Status:** Accepted
- **Date:** 2026-09-02
- **Owners:** Portal platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

ADR-0054 confines QindaQt's portal implementation to the standard Settings
backend. The original selector used a wildcard fallback, which allowed any
installed provider to become authority for any other family without an
auditable QindaQt choice. That makes package additions capable of opening new
desktop behavior and obscures which provider order applications receive.

QindaQt needs ordinary chooser, application chooser, access, mail, inhibit,
notification, print, screenshot, screencast, and remote-desktop frontend
routing, but this milestone adds no backend implementation or consent UI.
Background must remain closed. OpenURI is handled inside
`xdg-desktop-portal` and has no desktop-backend selector.

## Decision

The QindaQt selector will use `default=none`. Settings selects only the
`qindaqt` backend. Each reviewed non-Settings backend interface explicitly
prefers `kde;gtk;lxqt`; Background explicitly selects `none`. Provider absence
is fail-closed after the ordered list is exhausted. QindaQt's `.portal` file
continues to advertise Settings alone.

Selection evidence runs the real portal frontend only on a private bus and
with a staged metadata directory. An injected service owns the staged KDE
backend name and exposes only FileChooser, proving the declared first match
without starting a host backend. The same proof requires Background failure
and non-QindaQt selection under another desktop identity.

## Consequences

- Installing a new `.portal` provider cannot open an unlisted family for the
  QindaQt desktop.
- Distributions may satisfy reviewed families with KDE, GTK, or LXQt backends
  in that order without granting QindaQt implementation authority.
- A listed family is unavailable when none of those providers advertises it;
  this is preferable to an implicit provider with unreviewed behavior.
- New portal families require an explicit selector entry, tests, and a review
  of UI, consent, and fallback implications.
- The policy depends on `xdg-desktop-portal` frontend selection semantics but
  does not change the Settings wire contract accepted in ADR-0054.

## Revisit when

Revisit if QindaQt implements another backend family, the frontend changes its
selection language, a preferred provider is no longer maintained, or a family
needs a QindaQt-specific consent or security policy.
