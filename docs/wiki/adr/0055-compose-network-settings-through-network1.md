# ADR-0055: Compose Network settings through Network1

- **Status:** Accepted
- **Date:** 2026-08-31
- **Owners:** First-party applications and Platform Network
- **Supersedes:** None
- **Superseded by:** None

## Context

The Settings Center needs to show useful network state and a deliberately
small set of safe actions without becoming another NetworkManager authority.
The resident [Network service](../architecture/network-service.md) already
owns the bounded, secret-free Network1 contract, exact-owner lineage, and the
only permitted libnm dependency. Bypassing that boundary from a page would
duplicate owner and mutation policy, expose implementation details, and make
credential handling part of an ordinary settings process.

The route must also remain truthful across process replacement and ambiguous
mutation outcomes. An operation reply is not a new inventory snapshot, and a
credential prompt cannot be implemented through Network1 because its public
values and operations intentionally cannot carry secrets.

## Decision

The Network Settings route composes one public Network1 Qt transport, client,
and route projection for the lifetime of `qindaqt-settings`. The executable
passes only the route projection into QML. The route does not include private
Network service or adapter headers, link libnm, call D-Bus directly, or inspect
NetworkManager objects.

The projection derives inventory and owner/epoch/revision truth from the
public client. Owner loss or replacement immediately removes the retired
owner's inventory. A public degraded snapshot may retain bounded inventory as
explicitly stale and read-only. Operation success requests a fresh snapshot;
it never manufactures inventory truth. An uncertain result is visible and is
never replayed automatically.

The route exposes only reload, scan, connection of an already stored known
network, and disconnection of an active device, each gated by the public
client's current intent admission. It does not expose radio mutation, profile
creation or editing, or arbitrary operation parameters.

Network1 remains credential-free. The route provides no credential fields or
secret callback. Connecting a stored secured network is a bounded activation
intent; NetworkManager may consult an independently registered external secret
agent. If no such agent can satisfy the request, the route reports the bounded
failure and cannot prompt or retry with credentials.

The route remains a compiled Settings component using QST-1 and
QindaQt.Controls. Its QML module is installed with the Settings runtime, and
hostile route identifiers are rejected before the transport or model is
constructed.

## Consequences

Network Settings shares the resident service's exact-owner fencing, bounded
values, redaction, permission truth, and fixed operation vocabulary. It cannot
offer first-use network enrollment or repair a stored profile. Wi-Fi and WWAN
radio state is observable but intentionally read-only in this UI slice.

Tests must cover owner loss and replacement, stale read-only projection,
uncertain no-replay behavior, capability removal, credential-shaped source
poisons, responsive keyboard/accessibility behavior, and relocated installed
route construction. Package and source checks must fail if the route gains a
private service dependency, direct D-Bus/libnm access, credential input, or a
radio mutation entry point.

## Revisit when

Reconsider the operation surface only after a separate accepted contract owns
profile editing or credential-agent interaction, including its authentication,
privacy, lifetime, cancellation, and accessibility behavior. Live physical
hardware qualification may strengthen evidence but does not by itself broaden
this route's authority.
