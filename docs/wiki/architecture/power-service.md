# Power and brightness architecture

This page defines the accepted architecture for QindaQt power state, session
power actions, idle reporting, and brightness. Its current maturity is
**EXECUTABLE (PB-2 upstream adapters)**: the PB-0 pure values, PB-1 resident
service/client, and production UPower, power-profiles-daemon, logind-session,
logind-action, and injected-sysfs adapters are implemented with focused
private-bus evidence. A production shell Power applet consumes the public
client boundary. Idle, keyboard-backlight integration, the KWin backlight
provider, and PB-3 session-action presentation remain pending.

The durable choices are split across
[ADR-0023](../adr/0023-split-power-authority-across-service-and-shell.md),
[ADR-0024](../adr/0024-route-brightness-through-power1.md), and
[ADR-0025](../adr/0025-arbitrate-session-bound-power1-activation.md). The
production adapter boundary and the refined direct-sysfs primitive are in
[ADR-0056](../adr/0056-confine-production-power-upstreams.md).

PB-0 fixed bounded values, hostile-input validation, canonical byte codecs,
fixed QtDBus structures, and deterministic pure battery aggregation in
[`power_protocol`](../reference/power1-v1.md). PB-1 added the resident
`org.qindaqt.Power1` service and asynchronous client. PB-2 replaces its
unavailable test collaborators only when the composition root explicitly
selects production; the wire contract is unchanged.

## Authority map

| Concern | Truth authority | QindaQt owner |
| --- | --- | --- |
| Batteries, AC/UPS state, estimates | UPower | `Power1` collaborator and typed snapshot |
| Power profiles and holds | power-profiles-daemon | `Power1` collaborator |
| Suspend, hibernate, reboot, power off | systemd-logind | Shell session-action controller |
| Caller-relative `Can*` authorization | systemd-logind | Shell controller; never cached in `Power1` |
| Power/suspend/hibernate keys | logind `handle-*` inhibitor locks | Shell controller |
| Idle hint | compositor idle protocol plus logind | `Power1` idle collaborator |
| Internal-panel brightness inventory | Kernel backlight sysfs | Injected-root adapter; bounded read and truthful writability |
| Internal-panel brightness requests | No Power1 v1 method | Tested direct-sysfs primitive only; no public dispatch yet |
| External brightness changes | Kernel `actual_brightness` | Adapter re-reads observed truth after a write |
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
| `power_service` | Resident ownership, collaborator orchestration, and confined platform adapters | PB-2 production upstreams implemented |
| `power_client` | Exact-owner asynchronous snapshots and operations | PB-1 implemented |
| `power_backlight_provider` | Identity gate, KWin binding, external observation, Wayland teardown | Pending later slice |
| `power_idle` | Compositor-idle observation and logind idle hints | Pending later slice |
| [`brightness_model`](brightness-model.md) | Pure display/keyboard brightness composition on injected values | PB-0 candidate |
| [`power_applet`](../shell/power-applet.md) | Shell-private public-client projection, compiled panel interaction, and capability-gated operation dispatch | Production consumer of PB-1; no platform maturity claim |

The service coordinator may not own UPower, logind, profile-daemon, or sysfs
transport objects. Dedicated adapters own those resources behind the injected
PB-1 collaborator boundaries; the composition root owns adapter lifetimes.
Power modules do not link Display implementation modules or Wayland. Only the
later PB-5 binding may consume the public Display client.

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
- Battery-supply and keyboard-backlight identities take precedence over
  profile-hold identities in the shared Power1 handle namespace regardless of
  collaborator fact arrival order. The coordinator retains separately
  sanitized profile truth while a collision suppresses its public projection,
  re-evaluates that truth on each accepted battery identity change or
  authoritative battery unavailability, and restores it when the collision
  clears. Intrinsically malformed or unavailable profile input is not retained
  for this recovery. A collision degrades only the profile domain; valid
  battery and session truth remains published.
- The deterministic PB-1 collaborators remain the explicit `unavailable`
  mode. They publish `Unavailable/upstream-not-integrated` with zero
  capabilities and never open an upstream bus. The installed descriptor and
  user unit explicitly select `production`; the bare executable defaults to
  `unavailable` so an unconfigured invocation is fail-closed.
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

## PB-2 production upstream adapters

Production composition injects one bus connection and one sysfs root. UPower
provides the root `OnBattery` property and an atomic enumeration of battery,
UPS, and line-power devices. Line power contributes AC truth only through its
`Online` property. Only battery/UPS devices with `PowerSupply=true` enter the
system-supply inventory, so peripheral batteries cannot affect PB-0
aggregation; `IsPresent` is consulted only for batteries. The adapter accepts
only exact property types and known state/level/warning ordinals; zero time
estimates remain unknown, signed energy rate becomes its absolute magnitude,
and a malformed or disappearing device withdraws the complete battery domain.
Every multi-call refresh is pinned to one resolved unique owner.

Power Profiles prefers the modern
`org.freedesktop.UPower.PowerProfiles` name, path, and interface, then falls
back to `net.hadess.PowerProfiles`. `ActiveProfileHolds` (legacy `Holds`) is
projected without exposing daemon cookies. Hold acquisition uses the standard
three-string `HoldProfile` call and records its unsigned cookie only inside the
adapter for a later `ReleaseProfile(cookie)`.

The logind session adapter atomically combines manager properties with
`ListInhibitors`, discarding UID and PID before publication. `PrepareForSleep`
updates observed truth and resume triggers a fresh read. A separate action
authority queries `CanPowerOff`, `CanReboot`, `CanSuspend`, and
`CanHibernate`; the published admitted set is presentation truth, while every
submitted action re-queries its matching `Can*` from the exact current owner
immediately before dispatch. Only that per-operation `yes` is authoritative.
Action calls always use `interactive=false`, duplicate operation IDs never
redispatch, and owner loss during authorization or execution completes the
operation as uncertain. Power1 v1 has no session-action wire fields, so PB-3
must compose this boundary in the shell.

The sysfs adapter enumerates only below its injected root. It publishes exact
raw maximum and observed values, preferring `actual_brightness`, and reports
malformed, disappearing, or read-only devices with typed fail-closed truth. A
narrow write primitive is available only for a writable injected
`brightness` file and re-reads observation after the write. No setuid helper,
polkit prompt, fallback path, or host path exists in tests. Power1 v1 has no
display-brightness method, so this primitive is not remotely dispatchable.

## Production shell consumer

The production Power applet composes only the public `PowerClient` with the
pure brightness and presentation projections. A shell-private composition owns
the transport, client, and controller lifetime, then injects the controller—not
the transport or service—through the audited built-in host boundary. Its manifest requests
`power.read` and `power.control`; the same runtime policy evaluation that
resolves the applet gates client observation and mutations independently.

The controller accepts only a validated snapshot from the client's exact
current owner. Owner loss/replacement clears prior truth and makes any pending
operation terminal without replay. Profile and keyboard-brightness requests
are bounded, serialized, resolved against the current snapshot, and fenced by
request and generation lineage. Power1 v1 still defines no display-brightness
write. Compiled offscreen interaction and relocated installed-package tests
prove the renderer/host composition without contacting a user session bus,
power daemon, display server, or hardware.

The packaged process now selects production adapters. The applet receives
their validated public truth and remains capability-gated when any daemon or
device is absent. This is private-fixture evidence, not a claim about a
specific host's battery, profile daemon, logind policy, or backlight access.

## Internal-panel brightness

The implemented inventory adapter reads the configured backlight sysfs root
and owns a direct, permission-gated write primitive as decided by ADR-0056. It
does not inspect DRM, open `/dev/i2c*`, register a Wayland provider, or expose
a Power1 v1 display-brightness method. The topology and KWin registration
rules below remain the contract for a later provider slice.

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
| PB-2 | Production upstream adapters; backlight provider, idle, and session-bound activation continue separately | PB-1 plus routed supervisor contract |
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
consumer, and a source-policy boundary test that keeps core orchestration
host-free while rejecting Wayland, process/thread, cross-adapter transport,
and sibling-service dependencies.

PB-2 upstream verification adds registered `qindaqt.power-service-*` rows for
UPower enumeration/property changes/device removal/owner replacement and
hostile payloads; modern and legacy Power Profiles plus cookie holds; logind
session truth, inhibitors, sleep observation, actions, owner fencing, and
duplicate operation lineage; bounded sysfs enumeration/write/read-only cases;
and D-Bus activation of the built process against all fakes on a private
daemon. The production activation row is the build-root replacement for the
legacy activation row: it proves descriptor-triggered name activation, exact
unique-owner establishment, descriptor/unit contents, exit when the
constructing bus dies, and a fresh owner, epoch, and process on an independent
replacement bus. Scratch roots are under the assigned build tree. These rows
never use an ambient bus, `/sys/class/backlight`, hardware, polkit, or a desktop
session.

Deterministic continuation starts with hostile codecs, aggregation and model
properties; private-bus owner/epoch replacement; fake UPower/profile/logind
adapters; all-or-nothing inhibitors; LVDS/eDP/DSI counterexamples; exact
activation API signatures; retry exhaustion; and simultaneous-supervisor
convergence. Physical devices, polkit subjects, suspend/resume, DDC/CI, and
hardware hotkeys remain release evidence.

## Non-claims

This contract proves production adapter behavior only against injected private
services and fixture files. It does not claim a live host UPower,
power-profiles-daemon, logind policy, polkit subject, suspend/resume cycle,
physical backlight mutation, idle hint, keyboard backlight, KWin Wayland
provider, external monitor, hardware key, or host-session integration. The
logind action boundary has no Power1 v1 or shell presentation route, and the
sysfs write primitive has no public Power1 v1 operation. Those later slices
require their own executable and hardware evidence.
