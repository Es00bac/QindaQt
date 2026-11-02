# ADR-0435: QindaLink panel applet and the phone capability tokens

- **Status:** Accepted
- **Date:** 2026-11-02
- **Owners:** Shell
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0433](0433-qindalink-phone-companion-architecture.md) names a panel
applet, `qindaqt.applets.link`, as the desktop's at-a-glance view of the paired
phone and the place where a pairing code is compared and approved
([ADR-0434](0434-qindalink-pinned-tls-transport-and-sas-pairing.md)). The
public `org.qindaqt.Link1` API and `src/services/link_client` already exist.
Reading a phone's presence and battery, and approving a pairing, ringing or
unpairing it, are different authorities: the first is passive, the others
change trust or act on a personal device.

## Decision

Add a built-in panel applet composed by the shell like the Firewall and
Network applets, with two new capability tokens in manifest schema v1:

- `phone.read`: devices, connection, battery and network status, and the
  `PairingRequested` signal.
- `phone.control`: `ConfirmPairing`, `Ring` and `Unpair`. Effective only with
  `phone.read`.

The shipped policy grants both to the audited built-in and denies both to
third-party packages. A denied `phone.read` constructs no transport or client
and the applet presents permanently as Unavailable. A denied `phone.control`
keeps the read view and makes every action inert.

The applet consumes only the public `LinkClient`. It never starts a process:
"Open QindaLink" goes through the shell launcher's `activate` for the desktop
entry `org.qindaqt.Link.desktop` and is hidden when the launcher catalog has
no such entry. The pairing code is shown with the phone's sanitized name and
model, rendered as plain text, and Accept is never the default focus.

## Consequences

- `Capability` gains `PhoneRead` and `PhoneControl`; the manifest count guards
  and the built-in registry list `qindaqt.applets.link`.
- The `LinkClient` device parser reads an optional `caps` array into
  `LinkDevice::capabilities`. Link1 does not emit it yet, so "Ring phone"
  stays honestly disabled until the service reports the phone's advertised
  capabilities (a Link1 follow-up owned by the link service).
- Link1 has no pending-pairing property, so a request raised before the shell
  started is not shown; the service's own notification still carries it.
