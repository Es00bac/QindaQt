# ADR-0056: Adopt standard AppMenu and dbusmenu transports behind proof-bound ownership

- **Status:** Accepted
- **Date:** 2026-09-02
- **Owners:** Shell / global menu
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0033](0033-canonical-menu-model-and-authenticated-menu-ownership.md)
established a bounded canonical menu model and authenticated focused-window
ownership, deliberately deferring the legacy cross-toolkit transport. Existing
Linux toolkits commonly publish a numeric window id and dbusmenu object through
`com.canonical.AppMenu.Registrar`, then expose the menu through
`com.canonical.dbusmenu`. Those protocols are useful compatibility surfaces,
but the registrar permits a caller to name an arbitrary window and therefore
cannot replace QindaQt's focus, PID, or invocation proofs.

## Decision

QindaQt adopts the standard registrar and dbusmenu method, signal, property,
and value signatures behind three confined modules:

- a bounded registrar service binds each window to the D-Bus message's exact
  caller unique name, permits unregister/update only by that peer, and removes
  every binding on the exact owner's loss behind monotonic generations;
- an exact-owner asynchronous dbusmenu client treats signals as invalidations,
  accepts only complete bounded `GetLayout` snapshots above its revision high
  water, validates standard properties, and never retries an uncertain
  activation event; and
- a shell-neutral coordinator joins a numeric registrar id only through an
  injected compositor-authenticated mapping, re-runs ADR-0033 authentication,
  and republishes through the existing selector/exporter lineage.

The registrar's well-known name is requested only by an explicitly started
composition root on an injected connection. Neither construction nor a global
singleton owns the session bus. Production-shell instantiation remains a
separate change.

## Consequences

- Compatible applications can use the de facto standard wire protocol without
  granting a registration authority over another window.
- Remote dbusmenu revisions fence remote snapshot replay but do not authorize
  shell actions. Only ADR-0033's epoch/revision and invocation guard do that.
- Hostile layouts are rejected atomically under canonical depth/item/text
  limits and additional icon/property bounds. Unknown properties can evolve
  without widening QindaQt's model.
- An `Event` timeout is uncertain and cannot be retried automatically. The
  application may have acted even when the reply was lost.
- The runtime must inject authenticated focus facts, numeric-id mapping, the
  session connection, and applet facade. This decision adds no KWin/private
  compositor dependency and no shell-runtime side effect.

## Revisit when

The standard signatures require an incompatible extension, applications need
icon bytes in the canonical presentation model, a proven revision-bearing
property delta can safely replace full rereads, or the production focus mapper
can no longer provide an unambiguous numeric registrar id.
