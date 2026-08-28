# Power and brightness architecture

This page defines the accepted architecture for QindaQt power state, session
power actions, idle reporting, and brightness. Its current maturity is
**MODELLED**: authority, interfaces, failure behavior, and the implementation
order have passed independent review, but no `Power1` implementation or
user-facing power experience is claimed.

The durable choices are split across
[ADR-0023](../adr/0023-split-power-authority-across-service-and-shell.md),
[ADR-0024](../adr/0024-route-brightness-through-power1.md), and
[ADR-0025](../adr/0025-arbitrate-session-bound-power1-activation.md).

The PB-0 candidate now fixes bounded values, hostile-input validation,
canonical byte codecs, fixed QtDBus structures, and deterministic pure battery
aggregation in [`power_protocol`](../reference/power1-v1.md). Protocol tests
have focused executable evidence; aggregation remains candidate evidence until
its focused build/tests and independent exact-commit review pass. No service
maturity is claimed. Pure brightness composition remains the final PB-0 commit
boundary.

## Authority map

| Concern | Truth authority | QindaQt owner |
| --- | --- | --- |
| Batteries, AC/UPS state, estimates | UPower | `Power1` collaborator and typed snapshot |
| Power profiles and holds | power-profiles-daemon | `Power1` collaborator |
| Suspend, hibernate, reboot, power off | systemd-logind | Shell session-action controller |
| Caller-relative `Can*` authorization | systemd-logind | Shell controller; never cached in `Power1` |
| Power/suspend/hibernate keys | logind `handle-*` inhibitor locks | Shell controller |
| Idle hint | compositor idle protocol plus logind | `Power1` idle collaborator |
| Internal-panel brightness requests | KWin external-brightness protocol | `Power1` backlight provider applies through logind |
| External brightness changes | Hardware/provider observation | Provider commits observed truth back to KWin |
| Adaptive brightness | KWin | QindaQt exposes no competing adaptive loop |
| Keyboard backlight | UPower keyboard-backlight interface | `Power1` collaborator |
| External-monitor brightness | No v1 authority | Honest unavailable; PB-6 is reserved |

`Power1` holds no inhibitors in version 1. Lock-before-sleep remains a
KWin/KScreenLocker responsibility. Shell session actions acquire all three
`handle-power-key`, `handle-suspend-key`, and `handle-hibernate-key` locks as
one transaction: partial acquisition releases every acquired lock and exposes
no key action. Losing any lock unregisters all three actions before retry.

## Process and wire boundary

One bounded, D-Bus-activated `org.qindaqt.Power1` process publishes a fixed,
versioned snapshot. There is no separate `Brightness1` process. Planned public
values include:

- schema version, service epoch, monotonic revision, availability, bounded
  capability and diagnostic values;
- aggregate battery plus at most eight additional power supplies;
- at most four profiles and eight profile holds;
- a sanitized inhibitor summary that never exposes UID or PID;
- bounded keyboard-backlight and internal-backlight device values;
- typed operation results carrying initiating and observed lineage; and
- the exact child Wayland socket, protocol version, and binding epoch used by
  the backlight provider.

Every upstream owner replacement starts a new epoch and invalidates handles.
Timeout or authority loss after dispatch is uncertain: clients resnapshot and
never replay an operation automatically. The process exits on permanent
session-bus loss or Wayland loss and retains no platform handles across a
restart. Version 1 persists no power or brightness state in Settings1.

Planned modules keep values, orchestration, adapters, clients, and pure
composition separate:

| Module | Cohesive responsibility |
| --- | --- |
| `power_protocol` | Bounded values, codecs, validation, result lineage, pure battery aggregation |
| `power_service` | Resident ownership and collaborator orchestration only |
| `power_client` | Exact-owner asynchronous snapshots and operations |
| `power_backlight_provider` | Identity gate, logind apply, external observation, Wayland teardown |
| `power_idle` | Compositor-idle observation and logind idle hints |
| `brightness_model` | Pure display/keyboard brightness composition on injected values |

The service orchestrator may not own UPower, logind, profile-daemon, Wayland,
or sysfs transport objects. Power modules do not link Display implementation
modules. Only the later PB-5 binding may consume the public Display client.

## Internal-panel brightness

The provider reads `/sys/class/backlight` and `/sys/class/drm` strictly
read-only and writes brightness only through
`org.freedesktop.login1.Session.SetBrightness`. It never writes sysfs and
never opens `/dev/i2c*`.

Registration is fail-closed. One device is exposed only when both conditions
hold:

1. exactly one eligible backlight remains after the kernel type preference
   `firmware` over `platform` over `raw`; and
2. exactly one connected internal connector exists in KWin 6.6.5's internal
   set: LVDS, eDP, or DSI.

Zero or ambiguous devices/connectors publish typed unavailable reasons and
register nothing. EDID cannot disambiguate KWin's internal-output path and is
not a matching input. In particular, one eDP plus one DSI panel is ambiguous
regardless of enumeration order.

The injected device port has three operations: total discovery, one
lineage-stamped apply, and an owned external-change subscription. Hardware or
firmware changes commit `set_observed_brightness`; they never overwrite the
compositor request. Disappearance closes the subscription and destroys the
registered protocol device.

## Session-bound activation

The existing `qindaqt-session` supervisor owns publication because it is alive
after the child compositor socket exists. For each generation it:

1. proves the sanitized child socket exists under the private runtime root;
2. calls systemd user-manager
   `org.freedesktop.systemd1.Manager.SetEnvironment(as)` and the D-Bus daemon
   `org.freedesktop.DBus.UpdateActivationEnvironment(a{ss})`, publishing equal
   `WAYLAND_DISPLAY` and `QINDAQT_SESSION_WAYLAND_SOCKET` values;
3. awaits both replies; and
4. activates `org.qindaqt.Power1` only after publication succeeds.

The budget is one initial attempt plus at most two retries per generation.
Publication failures consume that same budget and never admit activation.
`Power1` refuses to connect unless both environment values are equal and name
an existing socket inside `XDG_RUNTIME_DIR`; there is no default-socket or host
fallback.

Because one user-level service name and activation environment are shared,
same-user graphical sessions use one deterministic arbiter. The active
graphical session wins; an unresolved tie selects the lowest active-or-online
session ID. Losers publish `unavailable(multi-session-loser)` and perform no
environment write, activation, retry, or `StopUnit`. Only the winner may
replace a foreign binding, preventing reciprocal restart loops.

## Vertical slices

| Slice | Outcome | Gate |
| --- | --- | --- |
| PB-0 | Protocol values, pure aggregation, pure brightness model | None; three reviewable commits |
| PB-1 | Wayland-free service/client, upstream collaborators, activation package | Accepted PB-0 |
| PB-2 | Backlight provider, idle collaborator, session-bound activation | PB-1 plus routed supervisor contract |
| PB-3 | Shell session actions and all-or-nothing key inhibitors | Shell owner and Controls/overlay boundary |
| PB-4 | Display D7 class-B brightness policy/method | Accepted Display D2 |
| PB-5 | Display-client binding, shortcuts, Power/Brightness Settings routes | PB-2, PB-4, shared app routes |
| PB-6 | Lid override, idle actions, charge thresholds, DDC/CI | Reserved; later ADRs required |

Deterministic verification starts with hostile codecs, aggregation and model
properties; private-bus owner/epoch replacement; fake UPower/profile/logind
adapters; all-or-nothing inhibitors; LVDS/eDP/DSI counterexamples; exact
activation API signatures; retry exhaustion; and simultaneous-supervisor
convergence. Physical devices, polkit subjects, suspend/resume, DDC/CI, and
hardware hotkeys remain release evidence.

## Non-claims

This contract does not prove an executable service, user interface, physical
device, session action, inhibitor, idle hint, or brightness mutation. It does
not complete Platform services. Progress may advance beyond MODELLED only
after accepted implementation and the evidence tier required by the relevant
slice.
