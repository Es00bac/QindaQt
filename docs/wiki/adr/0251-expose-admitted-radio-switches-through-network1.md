# ADR-0251: Expose admitted radio switches through Network1

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** First-party applications and Platform Network
- **Supersedes:** The radio-mutation prohibition in [ADR-0055](0055-compose-network-settings-through-network1.md)
- **Superseded by:** None

## Context

Network Settings shows Wi-Fi and mobile radio state but originally offered no way
to change it. The public Network1 contract now has typed `SetRadio` admission
and a service/libnm adapter. Leaving the route read-only forces users outside
Settings even though the authority already exists.

## Decision

The route exposes one switch per Wi-Fi and mobile radio. It sends only the
public `RadioKind` and requested boolean through `NetworkClient::setRadio`.
The public model and service retain hardware-block, presence, capability,
lineage, and operation admission. QML never calls D-Bus or private adapter
interfaces and does not accept arbitrary operation parameters.

Switches show confirmed software state from the latest accepted snapshot.
The route disables them when the radio is absent, hardware-blocked, capability
denied, stale, refreshing, or awaiting a change. An operation reply alone does
not change the displayed state. A successful reply starts bounded readback:
only a newer same-owner, same-epoch snapshot confirming the requested state
reports success. Stale readback retries, then becomes visibly uncertain at a
deadline. A newer contradictory snapshot reports conflict. Refusal, owner
replacement, and uncertain replies retire the request without replay. Refresh
only reads; the user must deliberately retry a write.

## Consequences

This supersedes ADR-0055 only where it forbade radio mutation. The
credential-free, profile-safe Network1 boundary still applies. Source-shape
checks permit precisely the typed radio invokable and reject private imports,
credential fields, and widened operation surfaces. Fake-transport and actual
QML keyboard tests cover admission, refusal, readback, conflict, owner change,
and no-replay behavior. Physical radio qualification remains separate from
these deterministic contract tests.
