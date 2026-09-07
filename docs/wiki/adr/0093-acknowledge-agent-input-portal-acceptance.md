# ADR-0093: Acknowledge agent input only after portal acceptance

- **Status:** Accepted
- **Date:** 2026-09-06
- **Owners:** Platform tools
- **Supersedes:** None
- **Superseded by:** None

## Context

A successful write to `qindaqt-agent-input` stdin only proves that a caller
handed bytes to a pipe. It does not prove that the approved RemoteDesktop
session accepted the event. Gabbee needs a bounded result before it can report
that its terminal fallback reached the portal path.

## Decision

`qindaqt-agent-input` accepts an optional, non-empty string `requestId` in an
input JSON event. For such an event it writes exactly one newline-delimited JSON
reply on stdout after dispatch: `{ "requestId": "…", "ok": true }` when the
synchronous `Notify*` portal call returns, or `{ "requestId": "…", "ok":
false, "error": "…" }` when the event is rejected or the portal call fails.

The acknowledgement means the approved portal accepted the input event. It is
not evidence that a target application consumed text or changed its document.
Callers must wait for the matching acknowledgement and report failure or
uncertainty instead of treating stdin flush as delivery. Events without a
`requestId` retain the existing fire-and-forget CLI behavior.

The existing per-session native portal approval remains mandatory. The helper
creates no consent bypass, persisted grant, clipboard path, or QindaQt-specific
portal discovery override.

## Consequences

- Gabbee can make its terminal fallback depend on a matching portal acceptance
  acknowledgement while normal QindaQt RemoteDesktop discovery and consent stay
  unchanged.
- Portal transport failures are observable by the initiating request rather
  than being mislabeled as delivered text.
- A fake private-bus portal test covers one successful and one failed
  acknowledgement. Live terminal/PTY qualification remains a separate
  user-session test because it needs an approved portal and a real terminal.

## Revisit when

Revisit if the RemoteDesktop portal gains asynchronous input completion
receipts, or if the helper adopts an EIS transport with a different delivery
contract.
