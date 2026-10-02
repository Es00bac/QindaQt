# ADR-0331: Fence native critical-battery countdowns and public actions

- **Status:** Accepted
- **Date:** 2026-10-01
- **Owners:** Power1, SessionActions and native sleep
- **Supersedes:** None
- **Superseded by:** None
- **Builds on:** [ADR-0330](0330-gate-native-source-profile-holds.md), [ADR-0325](0325-additive-native-sleep-modes.md)

## Context

The PF2 critical-battery action needs a cancellable countdown, while packaged
sessions still retain PowerDevil. Power1 already publishes authenticated UPower
warning/source facts and Settings1 stores the bounded action/countdown. Sleep1
already protects Hibernate as well as Suspend; SessionActions exposes only
Suspend to its consumers. A policy must not bypass protected sleep, replay an
uncertain mutation, or let delayed notification/capability callbacks outlive
cancelled intent.

## Decision

PowerService composes a cohesive CriticalBatteryPolicy over borrowed public
SettingsClient, current coordinator facts, a dedicated public SessionActions
client and a separate confined standard notification producer adapter. No Power1
wire/schema or idle-scope value changes. SessionActions appends Hibernate to its
public enum and availability/request surface, preserving old enum values, and
uses the same exact Sleep1 owner equals Session1 join and fresh CanHibernate
query as Suspend. PowerOff retains its reviewed current-owner noninteractive
logind route in SessionActions; policy never imports platform/private headers.

A separate `--critical-policy=native-exclusive` opt-in defaults off. It reuses
the subscribe-before-query canonical PowerDevil-name guard; no packaged or
supervisor cutover occurs. Only confirmed Supplies/on-battery/present-battery
and WarningLevel Action admit an attempt. Critical/Low/Unknown do not. Ready,
current-owner Settings values admit none/suspend/hibernate/power-off and 5–300
seconds; unconfirmed refresh cannot authorize pending work.

One monotonic countdown starts after a confirmed actionable notification. Each
second updates that same owned ID. Once started, the attempt consumes its
critical episode: user cancellation/dismissal, preference refresh/change,
provider/Settings/legacy/action-authority loss cancels it without restarting
while the same Action episode persists. Only authenticated AC or known warning
recovery resets the episode. Unknown/lost facts cannot synthesize recovery.
Admission withdrawal synchronously stops a dedicated pending SessionActions
client before a queued Can reply can dispatch. Dispatched uncertainty quarantines
automatic action for the entire policy lifetime; no retry or replacement queue.

The adapter uses public freedesktop Notify/CloseNotification and exact-owner,
ID and per-notification nonce checks for ActionInvoked. It requires actions
capability, serializes updates/closure, and closes only its own ID on its
original unique owner, including a late confirmed reply after cancellation.
Confirmed closure and a fresh policy-lineage check precede action submission.
A lost initial Notify reply supplies no safe close ID: it suppresses action and
resubmission. Every Notify requests explicit positive remaining+3-second expiry;
QindaQt's notification domain honors positive critical expiry. The standard
protocol cannot invent unknown-ID cleanup, and an unresponsive/nonconforming
foreign host cannot provide a closure guarantee. No initial/action timeout
silently replays the policy.

## Consequences

- PowerService adds a private public-SessionActions dependency; no reverse
  dependency or transport-private access. Notification implementation remains
  confined to its own adapter, not the core countdown state machine.
- Public SessionActions Hibernate availability comes from actual authenticated
  current-owner CanHibernate replies, with existing caller compatibility.
- Private runtime acceptance must use the actual resident subprocess, real
  Settings1 and notification host/user-action boundary, and injected protected
  Sleep1/logind action fixtures. No machine action/PAM/live notification proof.
- Source-profile/balanced behavior remains unchanged. Lid/full idle policy,
  PowerDevil retirement and installed/physical qualification remain separate;
  supported idle scopes remain zero.

## Revisit when

Final packaging can admit native-exclusive policy after every remaining native
power consumer and installed/hardware behavior is qualified.
