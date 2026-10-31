# Firewall1 protocol 1

`org.qindaqt.Firewall1` is owned by [QindaFirewall](https://github.com/Es00bac/QindaFirewall)
(a separate project), not by this repository. QindaFirewall's background
monitor (`qindafirewall-monitor`) publishes it on the session bus while it is
running; the service is absent when the user has not enabled it. This page
mirrors QindaFirewall's own `docs/monitor.md` ("AGENT-CONTRACT: D-Bus
`org.qindaqt.Firewall1`") so the [Firewall applet](../shell/firewall-applet.md)
has one place to point at; QindaFirewall is the source of truth for any
future change to the table below, and a schema change there must land in
QindaFirewall first.

Session bus, name `org.qindaqt.Firewall1`, object `/org/qindaqt/Firewall1`,
interface `org.qindaqt.Firewall1`. Structured values travel as JSON text, so
no client needs custom D-Bus types.

| Member | Kind | Meaning |
| --- | --- | --- |
| `State` | property, `s` | LogWatcher state: `off` `starting` `live` `denied` `retrying` |
| `Headline` | property, `s` (JSON) | SecurityMonitor headline `{level, title, text}` |
| `AlertCount` | property, `u` | alerts not dismissed |
| `TopAlert` | property, `s` (JSON) | the most serious open alert (SecurityMonitor alert map) or `""` |
| `Version` | property, `s` | program version |
| `Alerts()` | method → `s` (JSON array) | every open alert, most serious first |
| `Alert(id)` | method → `s` (JSON) | one alert or `""` |
| `Dismiss(id)` | method | "Got it, hide this" |
| `ConfirmMine(id)` | method | "This was me" (also expects future logins from that address) |
| `OpenWindow(id)` | method | start the window on that alert (`""` = overview) |
| `Changed()` | signal | any property may have changed |
| `AlertRaised(s)` | signal (JSON) | a live alert appeared or got worse |
| `Responded(s)` | signal (JSON) | the responder acted (an automatic block, announced like an alert) |

## The alert map

`TopAlert`, `Alerts()`, and `Alert(id)` carry QindaFirewall's SecurityMonitor
alert map, decoded here into `QindaQt::Firewall::Alert`
(`src/services/firewall_protocol/include/qindaqt/services/firewall_protocol/firewall_types.h`):
`id`, `kind`, `severity` (`info`/`notice`/`warning`/`danger`), `severityLabel`,
`title`, `summary`, `meaning`, `advice`, `canBlock`, `canConfirm`, `address`,
`place`, `user`, `count`, `firstText`, `lastText`, `ongoing`, `remote`,
`evidence` (a string list), and `mac`. `title`/`summary`/`meaning`/`advice`
are already plain English; nothing in this repository adds a technical term
to them.

## The headline

`Headline` decodes to `{level, title, text}`, where `level` is one of
`ok`/`notice`/`warning`/`danger`/`unknown`. [Firewall applet
presentation](../shell/firewall-applet.md) treats any level string outside
that set, and the period before the first successful property fetch, as
`unknown` — never as a guessed `ok`.

## This repository's client

`src/services/firewall_protocol` (pure value types and JSON codec),
`src/services/firewall_client` (`FirewallClient` over an abstract
`FirewallTransport` seam — no owner/epoch lineage to fence, because the
applet never mutates anything), and `src/services/firewall_qt_transport`
(the production `QDBusServiceWatcher`-based transport, fetching all four
properties in one `org.freedesktop.DBus.Properties.GetAll` call) implement
the consumer side. See [Firewall applet](../shell/firewall-applet.md) for
how the panel applet projects this contract, and
[ADR-0431](../adr/0431-firewall-panel-applet-over-public-firewall1.md) for
the decision to add it.
