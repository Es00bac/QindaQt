# ADR-0306: Keep idle-inhibitor scopes and policy in Power1

- **Status:** Accepted
- **Date:** 2026-09-30
- **Related:** [ADR-0023](0023-split-power-authority-across-service-and-shell.md), [ADR-0293](0293-settings1-native-power-policy-and-powerdevil-import.md), [ADR-0304](0304-native-lock-observation-and-service-policy.md), [ADR-0307](0307-public-ordinary-fd-idle-observation.md), [ADR-0308](0308-native-lock-preferences-and-atomic-import.md)

## Context

The native idle path needs one owner for stages, inhibition and their interaction.
The planned stages are automatic lock, display off and idle suspend. The public
Power1 snapshot already publishes a privacy-bounded summary of logind inhibitors,
but version 1 acquires no caller-scoped lease and there is no cohesive native
stage policy. A compatibility method must not report an accepted inhibitor
unless every requested scope has a real consumer.

## Decision

Keep the bounded inhibitor registry and automatic idle-stage policy inside the
resident Power1 process. The registry recognizes three independent scopes:
automatic lock, display off and idle suspend. The policy evaluates the current
source's confirmed Settings1 preferences and the corresponding current inhibitor
set before it dispatches each automatic stage. A stage capability is advertised
only when its preference, observation and action path are composed and that
stage's inhibitors are actually consumed.

An acquire request names a bounded application, reason and nonempty set of
scopes. Power1 validates the complete set before creating any lease. If any
requested scope is malformed or unsupported, the request rejects atomically and
returns no cookie. It never grants a supported subset. Cookies are scoped to
the current Power1 service epoch and the exact D-Bus unique caller. Only that
caller may release its own cookie. The registry bounds total leases and per-
caller leases; it removes every lease when that unique owner disappears and
invalidates all cookies when Power1 restarts. A cookie is a same-session handle,
not a secret or a substitute for D-Bus caller identity.

The standard ScreenSaver Inhibit compatibility request maps atomically to all
three scopes. It succeeds only when all three are supported and accepted.
UnInhibit releases only the cookie owned by its exact caller. Other public
PowerManagement or portal adapters must use the same registry instead of
creating a parallel list.

Idle inhibitors suppress only their matching automatic stages. Manual lock and
critical-battery actions ignore idle inhibitors. A manual action is not converted
into an automatic idle stage by policy. In particular, inhibitor state never
weakens the lock authentication boundary or makes Protected optional.

Power1 owns the in-process idle policy and its stage admission; it does not claim
to veto a privileged external logind request or another process's direct action.
Any QindaQt-owned suspend dispatch that must follow a lock still waits for an
actual Protected receipt before invoking the action. A finite logind delay
inhibitor is bounded coordination, not a universal denial of an external or
privileged suspend.

## Consequences

- Power1's public protocol gains explicit per-scope support and owner-bound
  acquire/release operations. Method documentation, validation, the client,
  daemon introspection and protocol reference must change together.
- The daemon's current service epoch and unique-owner watcher are the only lease
  lifetime authorities. Stale-epoch or other-caller cookies reject without
  changing state.
- Capability bits remain false until the corresponding stage is consumed in the
  real policy path. An installed facade that has no complete consumer returns
  Unsupported and no cookie.
- Tests cover each scope independently, combinations, malformed/unsupported
  atomic rejection, per-caller release, owner disappearance, restart, capacity,
  stage suppression and the two actions that must ignore idle inhibitors.
- Runtime and documentation distinguish QindaQt-owned dispatch from external
  logind authority; no test or status claim implies a live host action.

## Alternatives rejected

- **Let Lock1 keep its own cookie table.** This creates a second lifetime and
  stage authority, so ScreenSaver compatibility can disagree with idle policy.
- **Accept the scopes Power1 happens to support.** Partial success would tell
  callers that an inhibitor is active when one of the requested automatic
  actions can still occur.
- **Use a logind inhibitor as the whole implementation.** One idle lock does
  not identify which QindaQt stage a caller requested and cannot gate the
  compositor-owned lock/display stages by itself.
