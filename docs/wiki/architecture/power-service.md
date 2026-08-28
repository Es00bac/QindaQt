# Power and brightness architecture

This page defines the accepted architecture for QindaQt power state, session
power actions, idle reporting, and brightness. Its current maturity is
**EXECUTABLE (PB-1)**: the PB-0 pure protocol/aggregation/brightness values
and the PB-1 Wayland-free resident service/client slice are implemented with
focused evidence; backlight, idle, session actions, and every live platform
adapter remain pending as recorded below.

The durable choices are split across
[ADR-0023](../adr/0023-split-power-authority-across-service-and-shell.md),
[ADR-0024](../adr/0024-route-brightness-through-power1.md), and
[ADR-0025](../adr/0025-arbitrate-session-bound-power1-activation.md).

PB-0 fixed bounded values, hostile-input validation, canonical byte codecs,
fixed QtDBus structures, and deterministic pure battery aggregation in
[`power_protocol`](../reference/power1-v1.md). PB-1 added the resident
`org.qindaqt.Power1` service and asynchronous client described below. No
backlight, idle, key-inhibitor, or host-upstream maturity is claimed.

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

| Module | Cohesive responsibility | State |
| --- | --- | --- |
| `power_protocol` | Bounded values, codecs, validation, result lineage, pure battery aggregation | PB-0 accepted candidate |
| `power_service` | Resident ownership and collaborator orchestration only | PB-1 implemented |
| `power_client` | Exact-owner asynchronous snapshots and operations | PB-1 implemented |
| `power_backlight_provider` | Identity gate, logind apply, external observation, Wayland teardown | PB-2 |
| `power_idle` | Compositor-idle observation and logind idle hints | PB-2 |
| [`brightness_model`](brightness-model.md) | Pure display/keyboard brightness composition on injected values | PB-0 candidate |

The service orchestrator may not own UPower, logind, profile-daemon, Wayland,
or sysfs transport objects. Power modules do not link Display implementation
modules. Only the later PB-5 binding may consume the public Display client.

## PB-1 resident service and client

PB-1 implements the Wayland-free resident slice over the PB-0 protocol:

- `power_service` composes three injected collaborator seams — battery
  (UPower authority: supplies, keyboard backlights, AC truth), profile
  (power-profiles-daemon authority: profiles and holds), and session (logind
  authority: lid/dock/sleep truth and sanitized inhibitors). Each seam is a
  generation-fenced Qt interface; real daemon adapters arrive in later slices
  and must not leak raw upstream identity through it.
- The coordinator owns publication and lineage. Every collaborator value is
  untrusted: text is sanitized and every candidate is validated before
  publication. Publication is atomic last-known-good — a malformed domain
  loses only its own content and capability bits while every other accepted
  domain is retained, and a whole candidate that still fails validation
  degrades to an empty validated fallback. Public handles are restamped with
  the published epoch at assembly, so any upstream authority replacement
  (which advances the nonzero-random epoch) invalidates every earlier handle
  and completes dispatched operations as `Uncertain` exactly once.
- The PB-1 process deliberately injects deterministic unavailable
  collaborators: it owns `org.qindaqt.Power1` and speaks exact Power1 over
  D-Bus, but publishes an honest `Unavailable/upstream-not-integrated`
  snapshot with zero capabilities rather than touching host UPower,
  power-profiles-daemon, or logind. The process exits on constructing-bus
  loss and keeps no platform handles across a restart.
- `power_client` binds to the exact unique owner, publishes only
  `validateSnapshot`-accepted snapshots, coalesces invalidations, fences
  regressed epochs, equal-revision contradictions, and stale-owner replies,
  serializes mutations with local preflight, applies bounded request
  timeouts, and never replays a timed-out or owner-interrupted mutation —
  those complete exactly once as `Uncertain` and the caller resnapshots.
- The package installs the executable, private D-Bus activation descriptor
  paired with a hardened systemd user unit (`Type=dbus`, system-service
  syscall filter, no device or network families), introspection XML, and the
  public protocol/client/service headers.

The exact wire method and signal surface is recorded in the
[Power1 reference](../reference/power1-v1.md).

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

PB-1 installs the private activation descriptor and systemd user unit paired
to the resident executable. The Wayland-socket environment publication,
arbiter, retry budget, and supervisor-owned activation above remain the PB-2
contract; PB-1 performs no environment write and refuses no socket because it
never opens one.

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

PB-1 verification is focused executable evidence: coordinator
publication/last-known-good/epoch rows, operation validation and exactly-once
completion rows, fake upstream adversarial rows (oversize, out-of-range,
duplicate and cross-domain handles, contradictory lid truth, control
characters, malformed outcomes, stale generations), private-D-Bus residency
(exact introspection signatures, delayed replies, owner replacement, name
theft), executable activation against a private `dbus-daemon` (honest
unavailable truth, constructing-bus-loss exit, fresh epoch on replacement),
an installed package/consumer gate that proves the descriptor and unit
resolve the packaged executable and staged public headers compile a clean
consumer, and a source-policy boundary test that rejects host UPower, logind,
Wayland, sysfs, process, thread, and sibling-service dependencies.

Deterministic continuation starts with hostile codecs, aggregation and model
properties; private-bus owner/epoch replacement; fake UPower/profile/logind
adapters; all-or-nothing inhibitors; LVDS/eDP/DSI counterexamples; exact
activation API signatures; retry exhaustion; and simultaneous-supervisor
convergence. Physical devices, polkit subjects, suspend/resume, DDC/CI, and
hardware hotkeys remain release evidence.

## Non-claims

This contract does not prove a live UPower, power-profiles-daemon, or logind
adapter, a backlight mutation, idle hint, session action, inhibitor, physical
device, user interface, or host-session integration. PB-1's resident process
honestly reports `upstream-not-integrated` until those adapters land. Progress
beyond the recorded PB-1 slice requires the evidence tier defined by the
relevant later slice.
