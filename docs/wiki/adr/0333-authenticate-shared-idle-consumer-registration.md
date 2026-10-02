# ADR-0333: Authenticate complete shared idle-consumer registration

- **Status:** Proposed; complete native stage composition remains separate
- **Date:** 2026-10-02
- **Related:** [ADR-0306](0306-power1-idle-inhibitors-and-stage-policy.md), [ADR-0316](0316-power-idle-state-receipt-authority.md), [ADR-0319](0319-supervisor-owned-native-idle-display-policy.md)

## Context

Power1 has bounded exact-owner/epoch idle leases, and native lock/display stages
consume its authenticated receipts. No production path calls setConsumedScopes,
so compatibility inhibition correctly remains Unsupported. The supervisor owns
native stage composition; Power1 cannot infer it from preferences or PID strings.

## Decision

Add RegisterIdleConsumers(expectedEpoch, scopes) to Power1. Authenticate the
actual current Session1 unique sender and its real bus-daemon UID. Require the
current nonzero Power1 epoch, explicit composition-only native-exclusive admission
and confirmed absence of the legacy PowerDevil writer. Admit only all three known
scopes or withdrawal, never a partial declaration. The trusted supervisor must
declare only genuinely composed consumers and withdraw on any consumer loss.

Subscribe before resolving Session1/legacy owners; resolve actual current owners
before calls instead of trusting queued notification arguments. Supervisor
replacement, Power1 epoch replacement, legacy arrival, withdrawal and native
admission loss revoke capabilities and leases. Cookie owner/epoch/capacity and
atomic acquisition rules remain unchanged. Installed service defaults off;
this declaration does not activate stage actions or retire another process.

The public same-thread IdleConsumerRegistrar borrows PowerClient and the same
constructing bus. It preflights current Session1 ownership and snapshot epoch,
fences callbacks and performs no automatic retry. Cancel/destruction withdraws
only its original unique Power1 owner/epoch, ordered after any declaration on
that connection. Method-reported acceptance is not scope authority; consumers
continue to use authenticated current-owner Power1 state receipts.

## Consequences and verification

Private-bus tests must prove default-off, wrong caller, stale/zero epoch,
partial/unknown mask, complete admission, exact lease ownership, supervisor and
upstream epoch replacement, legacy arrival, explicit withdrawal, admission loss,
public receipt refresh and cancellation before a late callback. Original Power1
publication/operation/client and idle/lid regressions remain required.

Complete dim/lock/display/suspend composition and compositor-owned scoped blank
provenance are subsequent delivery boundaries. This slice must not insert a
placeholder consumer or advertise support based on a stored setting. Lock-before-
sleep Protected receipts and own-only restoration remain mandatory.
