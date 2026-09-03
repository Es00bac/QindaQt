# ADR-0069: Confine network credential entry to a separate secret agent

- Status: Accepted
- Date: 2026-09-03
- Deciders: QindaQt architecture group
- Scope: NetworkManager credential prompting, storage flags, and Settings presence

## Context

Network1 deliberately exposes bounded secret-free connectivity truth and
activation of existing profiles. The Network Settings route consumes that
public boundary. NetworkManager nevertheless needs an interactive SecretAgent
for secured profiles whose required value is not already available. Adding
that exchange to Network1 or the Settings route would move credential payloads
through a public QindaQt protocol and reverse the accepted module boundaries.

## Decision

Implement `qindaqt-network-secret-agent` as a separate confined process. It
speaks only NetworkManager's standard AgentManager and SecretAgent contracts on
an injected system-bus connection. It authenticates every caller against the
current NetworkManager unique owner, verifies connection paths through that
owner, admits only bounded supported settings/hints, and prompts only when
`ALLOW_INTERACTION` is explicit.

Secrets are reply-only ephemeral values. The process has no persistence and no
QindaQt credential interface. Unchecked remember sets `NOT_SAVED`; checked
remember clears both `NOT_SAVED` and `AGENT_OWNED`, delegating storage to
NetworkManager. `SaveSecrets` and `DeleteSecrets` are authenticated typed-void
no-ops.

Publish `org.qindaqt.NetworkSecretAgent1` on the session bus only as a
presence name after successful AgentManager registration. Export no object on
that name. Settings may observe ownership of this one name and display
explanatory status; it may not receive credential text or call an agent method.

## Consequences

- Network1, its resident service, and its NetworkManager adapter remain
  secret-free and do not link this module.
- Settings gains one tightly constrained Qt D-Bus presence observer but no
  callable D-Bus or credential surface.
- NetworkManager-owner replacement must cancel outstanding prompts before
  re-registration.
- The process must be deployed and started separately for interactive secured
  connection activation.
- VPN secrets, certificates, and agent-owned persistence require later
  decisions rather than silent expansion of this interface.
