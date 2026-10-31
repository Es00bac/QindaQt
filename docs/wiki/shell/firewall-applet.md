# Firewall applet

`src/shell/firewall_applet` owns the panel Firewall applet,
`qindaqt.applets.firewall`. It is the desktop's panel view of
[QindaFirewall](https://github.com/Es00bac/QindaFirewall)'s background
monitor: whether it is watching, and in plain English, whether anything is
trying to get in. The durable decision is
[ADR-0431](../adr/0431-firewall-panel-applet-over-public-firewall1.md).

The applet is a read-only consumer of the public
[Firewall1 protocol](../reference/firewall1-v1.md), exactly like the Network
applet is a consumer of Network1 (ADR-0258). It adds no service or helper
work, and it has no control surface at all: the manifest requests only
`security.read`, there is no `security.control` token, and nothing in this
module can block, allow, confirm an automatic action, or edit a rule. The
three buttons the popup offers (open the real window, hide an alert, mark a
login as the owner's own) are the complete set QindaFirewall documents for
this contract.

Current maturity: **built-in composition with deterministic fake-transport
and offscreen keyboard evidence**. A live check against a running
`qindafirewall-monitor` has not been recorded yet.

## Modules

| Part | Path | Responsibility |
| --- | --- | --- |
| Pure values | `include/qindaqt/shell/firewall_applet/*`, `src/firewall_applet_presentation.cpp` | Project `(service absent, State, Headline, AlertCount, TopAlert)` into one of six states and their plain-English text. Links only the Firewall1 protocol values. |
| Runtime controller | `src/firewall_applet_controller.{h,cpp}` | Borrow the shell's `FirewallClient`, publish bounded QML values, forward the three button actions. |
| QML | `qml/FirewallApplet.qml` | Compiled `QindaQt.Shell.FirewallApplet 1.0` on the shared [controls](controls.md) and [panel popup](panel-popup-placement.md). |
| Composition | `src/shell/runtime/firewallappletcomposition.{h,cpp}` | Evaluate the audited manifest, registry, and policy once; own the Qt D-Bus transport and client (or construct neither, on a denied grant). |

The runtime target links `QindaQt::FirewallClient` only. The Qt D-Bus
transport is owned by shell composition, and the boundary test
(`qindaqt.firewall-applet-boundary`) rejects QDBus, the service, process
starts, and any text-entry control in the applet sources, and requires every
action to go through one of the three typed `FirewallClient` calls.

## The six states

Presentation (`projectFirewallApplet`) is deterministic over five inputs:
whether the service is present on the bus, the raw `State` property, the
decoded `Headline`, `AlertCount`, and `TopAlert`.

| State | When | Panel text (also the tooltip) |
| --- | --- | --- |
| Unavailable | service absent, or `security.read` denied | "Background watching is off. Open QindaFirewall to turn it on." |
| Blocked | `State == "denied"` | "QindaFirewall can't read the logs." |
| Secure | headline level `ok`, no open alerts | "All quiet. Nobody is trying to get in." |
| Watching | headline level `unknown` (including `State == "starting"`), or an inconsistent `ok` headline with `AlertCount` still nonzero | "Watching the logs…" |
| Attention | headline level `notice` or `warning` | the headline title, e.g. "Someone is trying to get in" |
| Alarm | headline level `danger` | the headline title, e.g. "Possible break-in — act now" |

Precedence: `denied` wins over every headline reading (a headline computed
before read access was lost is not current truth about an active
break-in), then Alarm, then Attention, then Secure, and Watching is the
deliberately uncertain catch-all for Unknown and the inconsistent case.
Service absence wins over everything: a stale `topAlert`/`alertCount` the
caller still passed in is never shown once `serviceAvailable` is false.

## The panel icon

The glyph is always `security-high`/`security-high-symbolic` (the symbolic
variant in the panel); only its theme-role tint changes, from a closed
vocabulary (`muted`/`success`/`warning`/`danger`) QML maps to QindaTK
tokens — never a literal colour:

| State | Tint role |
| --- | --- |
| Unavailable, Watching | `muted` (`Tokens.fg.muted`) |
| Secure | `success` (`Tokens.status.success.foreground`) |
| Blocked, Attention | `warning` (`Tokens.status.warning.foreground`) |
| Alarm | `danger` (`Tokens.danger.default`) |

An unread-count badge (`AlertCount`) sits on the icon's corner whenever it is
nonzero. Alarm alone additionally draws a fixed danger-coloured ring around
the icon; it is deliberately not an animation (reduced motion stays
reduced, and the fixed ring is simpler to reason about and to test than a
pulse). The tooltip and the summary button's accessible name are both the
state's panel text.

## The popup

With an open alert (`TopAlert` present), the popup shows its `title` as the
heading, then three labelled sections in QindaFirewall's own words:
**What happened** (`summary`), **Is this dangerous?** (`meaning`), and
**What should I do?** (`advice`). Three buttons follow:

- **Open QindaFirewall** — `OpenWindow(topAlert id)`. Always present.
- **Got it, hide this** — `Dismiss(topAlert id)`. Present whenever there is
  an alert.
- **This was me** — `ConfirmMine(topAlert id)`. Present only when the alert's
  `canConfirm` is true (QindaFirewall never offers this for a kind where
  marking a login as the owner's own would not make sense).

With no open alert, the popup shows the state's panel text and a single
**Open QindaFirewall** button, which calls `OpenWindow("")` — the documented
overview target. Every button click closes the popup. No control here is
destructive: the applet cannot block an address or change a rule, so there
is nothing to confirm before acting.

## Production seams

- Manifest `data/applets/firewall.json` requests `security.read`, a new
  read-only capability token in schema v1
  ([manifest schema](../reference/applet-manifest-schema-v1.md)). There is
  no `security.control`.
- `BuiltinAppletRegistry::firstParty()` lists `qindaqt.applets.firewall`.
- `ShellRuntimeApplication::initializeServiceAppletCompositions` builds the
  composition; `RuntimePanelWindowFactory::setFirewallAppletAccess` hands the
  controller to panel QML as `firewallAppletAccess`, and
  `BuiltinAppletContent.qml` renders it.
- No default profile places the applet yet; it resolves like every other
  built-in applet once a profile's panel names `firewall`, the same as the
  Network applet before ADR-0258 placed it in the default profile.

## Focused verification

| Row | Covers |
| --- | --- |
| `qindaqt.firewall-protocol-codec` | JSON decode of the headline and alert map: well-formed input, malformed/empty payload, a missing alert id, an unrecognized field ignored, one bad list element skipped rather than failing `Alerts()` whole |
| `qindaqt.firewall-applet-presentation` | all six states, the denied-wins-over-headline rule, the inconsistent-data fallback to Watching, service absence overriding a stale alert, plain-English fields copied verbatim |
| `qindaqt.firewall-applet-controller` | fake Firewall1: service appears/disappears retiring every fact, a denied grant never touching a null client, `AlertRaised` updating the count once the follow-up refresh lands, and each button calling exactly its one typed `FirewallClient` method |
| `qindaqt.firewall-applet-offscreen` | compiled QML under `QT_FATAL_WARNINGS`: the unavailable panel's accessible name and absent badge, the alarm popup's labelled sections, and each button click in its own isolated harness |
| `qindaqt.firewall-applet-boundary` | no QDBus or process reach, no text-entry control, only the three typed `FirewallClient` calls, no "block"-shaped verb anywhere in the controller |
| `qindaqt.applet-catalog`, `qindaqt.applet-runtime-resolution` | manifest, the `security.read` capability token, and registry resolution |

## Non-claims

- No live check against a real `qindafirewall-monitor` process is recorded
  yet; all evidence is the deterministic fake transport.
- The applet never shows `Alerts()`'s full list; the popup presents only the
  top alert, matching QindaFirewall's own `TopAlert` property. `Alerts()`
  and `Alert(id)` are implemented on `FirewallClient` for contract
  completeness but are not wired into any control.
- The responder, the incident log, and the Telegram channel
  (QindaFirewall docs/monitor.md) are entirely QindaFirewall's own; this
  applet only reads `Responded`'s pass-through signal and refreshes.
