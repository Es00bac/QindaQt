# ADR-0258: Network panel applet over public Network1

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** Shell
- **Supersedes:** None
- **Superseded by:** None

## Context

The desktop has never had a panel view of the network. Audio, Bluetooth,
Power, and Voice each have a panel applet; the network can only be seen and
changed in Settings. The public Network1 client (ADR-0045, ADR-0052,
ADR-0055) already offers snapshot truth, scan, joining saved and visible
networks, disconnect, and radio switching (ADR-0251), with typed intent
admission and uncertain outcomes. Credential entry is confined to a separate
secret agent (ADR-0069).

## Decision

Add a built-in panel applet, `qindaqt.applets.network`, as a second consumer
of the public `NetworkClient`, composed by the shell exactly like the
Bluetooth applet:

- A new manifest capability pair, `network.read` and `network.control`, gates
  observation and mutation. Control is effective only with read. System Status
  requests the same pair for its network lane.
- The shell owns one Qt D-Bus transport and `NetworkClient` for the applet.
  The applet's own targets link only the Network1 client, model, and protocol.
- The applet sends only typed intents: `setRadio`, `connectKnownNetwork`,
  `connectVisibleNetwork` with the opaque access-point id, `disconnectDevice`,
  and `requestScan`. It has no text-entry control and no credential field; a
  secured first-use network prompts in the secret agent.
- One request at a time. An accepted reply is not success: the applet reports
  success only when a newer snapshot from the same owner and epoch shows the
  requested state, within a bounded window. Refusal, owner change, transport
  failure, and an unconfirmed window end the request as failed or uncertain,
  and nothing is replayed.
- Rows exist only while the snapshot is current; owner loss clears them.
- "Network Settings…" opens the Settings `network` route through the shell's
  existing route launcher.

## Consequences

- No process boundary, service, or NetworkManager adapter changes. Network1
  gains a consumer, not an interface.
- The capability enum grows by two tokens; existing manifests are unaffected.
- The default profile places the applet next to Bluetooth and Power. Profiles
  that aggregate status place it through System Status instead, so a panel
  never shows the network twice.
- Hidden, enterprise, and WEP networks, VPN, hotspot, and wired-profile
  editing remain in Settings.
- Live hardware qualification is separate from the deterministic
  fake-transport and offscreen evidence.

See [Network applet](../shell/network-applet.md).
