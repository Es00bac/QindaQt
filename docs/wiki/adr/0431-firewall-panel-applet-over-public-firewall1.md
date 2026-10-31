# ADR-0431: Firewall panel applet over public Firewall1

- **Status:** Accepted
- **Date:** 2026-10-31
- **Owners:** Shell
- **Supersedes:** None
- **Superseded by:** None

## Context

[QindaFirewall](https://github.com/Es00bac/QindaFirewall) is a standalone
security app with its own background monitor (`qindafirewall-monitor`)
that watches the system log for break-in attempts and publishes
`org.qindaqt.Firewall1` on the session bus while running (QindaFirewall
`docs/monitor.md`). The desktop has never had a panel view of it: a user has
to open QindaFirewall's own window to learn whether anything is wrong. Audio,
Bluetooth, Power, Network (ADR-0258), and Agent Usage each have a panel
applet; security status does not.

Firewall1 is deliberately small and read-only from this repository's point
of view: four properties (`State`, `Headline`, `AlertCount`, `TopAlert`),
three signals, and five methods, three of which (`Dismiss`, `ConfirmMine`,
`OpenWindow`) are the complete set of actions a consumer may take. None of
them blocks or allows anything; QindaFirewall's own responder is the only
thing in that system that ever touches the firewall automatically, and it is
not reachable from this contract at all.

## Decision

Add a built-in panel applet, `qindaqt.applets.firewall`, as a public Firewall1
consumer, composed by the shell exactly like the Network and Agent Usage
applets:

- A new read-only capability token, `security.read`. There is no
  `security.control`: the manifest cannot request authority this contract
  does not expose, and the applet's own sources are boundary-tested to use
  only the three typed `FirewallClient` calls.
- `src/services/firewall_protocol` (pure value types and JSON codec —
  Firewall1's structured values travel as JSON text, unlike Network1's
  native D-Bus types), `src/services/firewall_client` (`FirewallClient`
  over an abstract `FirewallTransport`), and
  `src/services/firewall_qt_transport` (the production
  `QDBusServiceWatcher` transport) mirror the Network1 client/transport
  split. Unlike Network1, there is no owner/epoch lineage to fence: Firewall1
  has no mutation-conflict surface, because this applet never mutates
  anything. `serviceAvailable() == false` is first-class, because the
  background monitor is optional and off by default.
- Six presentation states
  (Unavailable/Blocked/Secure/Watching/Attention/Alarm) project
  deterministically from `(service absent, State, Headline, AlertCount,
  TopAlert)`. `denied` wins over every headline reading; Danger/Notice-or-
  Warning/Ok-with-no-alerts map to Alarm/Attention/Secure; anything else
  (an undecoded headline, or an inconsistent `ok` headline that still
  carries open alerts) falls back to Watching rather than guessing.
- A denied `security.read` grant constructs no transport and no client at
  all (the Agent Usage applet's precedent) and the controller presents
  permanently as Unavailable without ever touching a client.
- The popup shows the top alert's `title`/`summary`/`meaning`/`advice`
  (QindaFirewall's own plain-English wording, unchanged) under three
  labelled headings, and the three documented buttons: Open QindaFirewall
  (`OpenWindow`), Got it, hide this (`Dismiss`), and — only when the alert's
  `canConfirm` is true — This was me (`ConfirmMine`). With no open alert,
  only Open QindaFirewall remains, calling `OpenWindow("")`.
- The panel icon is the fixed `security-high`/`security-high-symbolic` glyph,
  tinted by a closed theme-role vocabulary (`muted`/`success`/`warning`/
  `danger`) rather than a literal colour, with an `AlertCount` badge and a
  non-animated attention ring reserved for Alarm.

## Consequences

- No process boundary, service, or QindaFirewall-side change. Firewall1
  gains a consumer, not an interface; any future change to the contract
  itself belongs to QindaFirewall, and `docs/wiki/reference/firewall1-v1.md`
  mirrors its table rather than owning it.
- The capability enum grows by one read-only token; existing manifests are
  unaffected, and `data/applet-policy/default.json` gains an explicit
  third-party deny rule for it (the Agent Usage precedent).
- No default profile places the applet yet; adding it to a stock layout is a
  separate, later decision.
- Live-hardware qualification against a running `qindafirewall-monitor` is
  separate from the deterministic fake-transport and offscreen evidence this
  change ships with.

See [Firewall applet](../shell/firewall-applet.md).
