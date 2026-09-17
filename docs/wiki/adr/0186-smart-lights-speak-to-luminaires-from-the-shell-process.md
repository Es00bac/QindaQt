# ADR-0186: Smart lights speak to luminaires from the shell process

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Shell and Platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt's other device-facing applets consume a resident D-Bus service: audio,
Bluetooth, power, network, display, and clipboard each own a system resource
that needs privilege, exclusive access, or arbitration between several
consumers. That pattern exists because the underlying resource cannot be shared
safely by every process that wants to read it.

Wi-Fi luminaires on the local network are not that kind of resource. A Wiz
light is reached with unauthenticated JSON datagrams on UDP 38899 from any
process that can open a socket; there is no device node to own, no privileged
capability to mediate, and no exclusivity to arbitrate. A resident service in
front of it would add a process, a bus name, an interface version, and a
packaging unit without gaining a single guarantee the socket does not already
give, while adding a failure mode: lights become uncontrollable when a service
that has no reason to exist fails to start.

The forces that do matter are different ones. The device is unauthenticated and
anyone on the broadcast domain can impersonate it, so decoded values must be
bounded. Capability truth must come from the device rather than from a guess,
because the same protocol serves dimmable, tunable-white, and colour
luminaires, and offering a colour control that silently does nothing is worse
than offering none. The user's own configuration (names, saved arrangements)
belongs to the desktop, not to the device.

## Decision

Smart-light control is composed in the shell process, behind the ordinary
audited-applet capability gate, and is layered as four separate modules:

- a **pure protocol** target that owns the wire format, the bounds every
  decoded value must satisfy, capability inference from the device's own model
  report, and the admission rules for a control intent;
- a **pure model** target that owns device inventory, reachability, and
  epoch/revision accounting, with no sockets and no timers;
- a **client** that owns discovery, interrogation, polling, and serialized
  control over injected transport and clock seams, with the only socket
  implementation in a separate target; and
- an **applet** that projects bounded rows and dispatches intents.

Two new capabilities, `smart-lights.read` and `smart-lights.control`, gate the
composition exactly as the Bluetooth capabilities gate theirs. Without the read
grant the client is never started, so no datagram — not even a discovery
broadcast — leaves the host. Third-party packages are denied both.

The user's own configuration is persisted by the applet to a single JSON
document under the desktop's configuration directory, not through Settings1,
because it is applet-owned data with no cross-process consumer and no schema
that another component needs to agree on.

## Consequences

- A luminaire becomes controllable as soon as the panel is running; there is no
  service ordering, activation, or restart story to get right.
- Everything except the socket is exercised without a network. The transport
  and clock seams make discovery, retry, timeout, reachability, and capability
  inference deterministic in tests.
- Nothing outside the shell can control the lights today. A second consumer —
  a Settings page, a scripting interface, a scheduled routine — is the concrete
  trigger for promoting this stack to a resident service; the protocol, model,
  and client targets are already independent of the shell for that move.
- The applet must keep its own bounds. Because no service stands between the
  network and the panel, the protocol target's limits are the only thing
  between a hostile datagram and a projected row.
- Losing the shell loses the polling loop. Lights keep whatever state they were
  last given, which is what a wall switch does too, but no desktop component
  observes them while the panel is gone.

## Revisit when

Reconsider when a second in-tree consumer needs the same inventory, when a
vendor protocol that does require privileged access or exclusive ownership is
added beside Wiz, or when measured polling cost in the shell process becomes
visible in panel latency.
