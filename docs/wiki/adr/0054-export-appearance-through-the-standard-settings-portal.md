# ADR-0054: Export appearance through the standard Settings portal

- **Status:** Accepted
- **Date:** 2026-08-31
- **Owners:** Portal platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt appearance truth already has two distinct owners: Settings1 owns the
confirmed user values and QST-1 derives the rendered semantic palette. A
sandboxed application cannot safely read either QindaQt's settings files or
private process state, while common toolkits already consume the standard XDG
Settings portal.

The installed xdg-desktop-portal 1.20.4 dependency provides a sufficient
backend interface and standard keys for color scheme, accent color, and
contrast. Creating a QindaQt-private application protocol would duplicate the
standard boundary, bypass toolkit integration, and invite source-truth drift.
Putting the backend in Settings1 itself would mix public portal compatibility,
activation metadata, and change-signal rules into persistence authority.

## Decision

QindaQt will run a separate D-Bus-activatable process owning
`org.freedesktop.impl.portal.desktop.qindaqt` and implementing only
`org.freedesktop.impl.portal.Settings` version 1 at the standard backend object
path. The installed `.portal` metadata advertises only Settings, and QindaQt's
portal configuration selects this backend for that interface while leaving
unimplemented portals to other providers.

The process consumes only a complete, Ready, exact-owner/epoch snapshot from
the public Settings1 client. A pure projector maps color-scheme directly,
derives accent from the selected theme through public QST-1, and combines the
theme variant with the high-contrast setting for standard contrast. It does
not read settings files, import Settings1 service internals, or use the legacy
accent schema value that first-party QST rendering does not consume.

Any owner loss, replacement without a complete baseline, malformed snapshot,
unknown theme, or failed/nonopaque QST derivation withdraws all readable
appearance truth. Because the standard has no removal signal, withdrawal emits
no fabricated change; a later baseline emits only values that differ from the
last signalled policy. Session-bus loss terminates the process rather than
reconnecting old lineage to a new daemon.

Policy/QST projection, Settings1 source state, D-Bus adaptation, and package
metadata remain separate. The package includes activation and hardened user
service descriptors plus the theme data needed for projection. It adds no
consent UI and claims no chooser, OpenURI, notification, inhibit, screencast,
or remote-desktop interface.

## Consequences

- Sandboxed and portal-consuming applications can observe QindaQt appearance
  through their existing standard Settings portal integrations.
- Settings1 remains persistence and revision authority; QST-1 remains semantic
  rendering authority; the portal owns only compatibility projection.
- Availability is intentionally fail-closed. Consumers may temporarily see no
  QindaQt values during Settings1 loss or replacement rather than stale policy.
- The resident process and its Settings1 client consume one session-bus name
  and one four-key bounded subscription, and may restart independently.
- Distribution integration must install/select both xdg-desktop-portal and the
  QindaQt backend package. Repository tests do not mutate or qualify host
  portal state.
- Extending the executable with unrelated portals is not implied by this
  decision and requires its own authority, threat model, UI, tests, and ADR.

## Revisit when

Revisit with a superseding ADR if the installed standard backend removes or
incompatibly changes these signatures, Settings1/QST cease to be QindaQt's
appearance truth, a standard removal signal is introduced, or evidence shows
that a separately activated Settings-only process cannot meet the session
resource/reliability budget.
