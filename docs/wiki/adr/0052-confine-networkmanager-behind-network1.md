# ADR-0052: Confine NetworkManager behind the resident Network1 process

- **Status:** Accepted
- **Date:** 2026-08-30
- **Owners:** Network platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0045](0045-fence-network1-pure-boundary.md) fixed a platform-free N0
protocol, model, and client boundary. N1 needs a real resident service and
NetworkManager transport without moving libnm handles, credentials, ambient
system-bus state, or restart ambiguity into that boundary. NetworkManager may
also replace its D-Bus unique owner while Network1 retains its public unique
owner; N0 deliberately rejects a same-owner epoch change, so treating such a
replacement as an in-process refresh would violate the accepted lineage gate.

## Decision

N1 is composed from three additional modules:

- `network_qt_transport` implements N0's `NetworkTransport` with Qt D-Bus,
  activates `org.qindaqt.Network1`, follows its exact unique owner, sends only
  fixed typed method arguments, and carries only canonical N0 byte payloads;
- `network_service` owns the public service name, boot-monotonic epoch,
  revisions, validation, serialized operation deadline, fixed D-Bus object,
  and an injected secret-free `NetworkBackend`; and
- `network_manager_adapter` alone links libnm, observes NetworkManager public
  state and permission results, normalizes it into bounded N0 values, and
  dispatches scan, stored-known-network activation, active disconnect, and
  Wi-Fi/WWAN radio requests through an injected port.

The resident process is user-session D-Bus activatable and also ships a
hardened user systemd unit. The unit permits only `AF_UNIX`; the adapter reaches
NetworkManager through libnm's system D-Bus connection and never opens a
network socket. All libnm and GObject handles remain private to the adapter and
are confined to the constructing Qt thread.

Network1 never requests `GetSecrets`, accepts a credential argument, or
republishes connection setting maps. Connect operates only on an already
stored NetworkManager connection. If credentials are required, NetworkManager
must obtain them from an independently registered external secret agent; that
agent and any credential-entry UI are outside Network1.

Initial NetworkManager absence publishes an honest bounded `Unavailable`
snapshot and may later recover. After a nonempty NetworkManager unique owner
has been observed, its disappearance or replacement retires the entire
Network1 process lineage. Pending dispatched work completes once as
`Uncertain`, the service publishes final unavailable truth, and exits with
status 75. The systemd unit or a client's fresh D-Bus activation starts a new
process with a fresh Network1 unique owner and a strictly higher
boot-monotonic epoch. Local session-bus loss likewise terminates the process;
the old broker and owner are never reused.

## Consequences

- N0 remains reusable without Qt D-Bus or libnm, and its source-policy proof
  remains unchanged.
- Production observation and permitted intent dispatch exist, but Network1 is
  not the authority for connection profiles, credentials, policy decisions,
  or hardware state.
- A service restart is intentionally visible to clients as owner loss,
  unavailability, and a new lineage. Mutations are never replayed across it.
- The `QindaQtNetworkN1` install component contains the three N1 libraries,
  resident binary, introspection XML, D-Bus service descriptor, and systemd
  user unit; it is composed with the N0 component for consumers.
- Tests use private session and empty private system buses plus injected fake
  NetworkManager facts. They do not mutate or qualify host radios, physical
  Wi-Fi/Ethernet, stored profiles, secret agents, or credential entry.

## Revisit criteria

Revisit with a superseding ADR before putting secrets or arbitrary setting
maps on Network1, changing the restart fence, allowing direct network sockets,
moving libnm into N0/service modules, or making Network1 the credential or
profile authority.
