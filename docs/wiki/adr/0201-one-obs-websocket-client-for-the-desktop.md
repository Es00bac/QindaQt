# ADR-0201: One obs-websocket client for the desktop

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Streaming (obs_client, OBS applet, Settings Streaming route)
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt drives OBS for recording, streaming and the virtual camera. OBS's
only supported control surface is obs-websocket v5, a JSON protocol over a
WebSocket on the local machine. Two QindaQt surfaces need it — the top-bar
applet and the Settings Streaming route — and a third party already speaks
it: the QindaQt OBS bridge plugin (ADR-0190) publishes the audio console's
buses and strips through an obs-websocket *vendor* request.

Written twice, this would be two reconnect policies, two request-id schemes
and two readings of the same protocol, drifting apart at exactly the moments
that matter (OBS closing mid-recording, a refused password, a bridge that is
not installed).

## Decision

**One `QindaQt::ObsClient` in `src/services/obs_client`, with the protocol as
pure functions and the socket behind a seam.**

- `obs_protocol` decodes and encodes frames and nothing else: no socket, no
  state, no timers. A frame this client does not speak — a malformed payload,
  a missing `requestId`, an opcode outside the table — is a fault reported to
  the caller, never an empty update applied as if understood.
- `ObsTransport` is the seam; `QtObsTransport` is the only implementation
  that opens a socket. **It accepts loopback addresses only.** obs-websocket
  has no transport security, so a host that pointed off the machine would put
  the password and every scene name on the network in the clear; the refusal
  lives in the transport rather than in each caller.
- The client owns one connection, one pending-request table keyed by an id it
  generates, and one published snapshot. State comes from OBS's events after
  one initial read, so an OBS the user drives directly stays reflected.
- **The one exception to "nothing polls":** elapsed time and dropped frames
  are published by no obs-websocket event and exist only in
  `GetRecordStatus`/`GetStreamStatus`. While an output is *active* the client
  re-reads that output's status on a slow timer. An idle OBS is never polled.
- A lost connection clears the live state. Leaving the last known status
  published would let the top bar show a recording that stopped when OBS quit.
- The console mapping is read **through the bridge's vendor request**
  (`qindaqt` / `GetConsoleMapping`, and its `ConsoleMappingChanged` event),
  never by parsing OBS source names. A source name is a display string the
  user can rename; the vendor payload carries the console id.

## Consequences

- `dev-qt/qtwebsockets` becomes a runtime dependency of the desktop.
- Both surfaces share the reconnect policy, the timeout policy and the
  reason-code vocabulary, so "OBS is not running", "OBS refused the password"
  and "OBS speaks a protocol we do not" read the same everywhere.
- Another plugin's vendor traffic reaches this client too; it is discarded
  unless the vendor name and payload type are the bridge's.
- The password is held only to answer a Hello challenge. It is never
  published in the snapshot, never logged, and never reaches the applet
  controller or any QML file.

## Revisit when

- obs-websocket publishes statistics as events, which would remove the one
  polling exception.
- OBS gains a control surface with transport security, which would make a
  non-loopback address a reasonable thing to offer.
