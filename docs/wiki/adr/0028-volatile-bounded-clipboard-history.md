# ADR-0028: Keep the clipboard history volatile, bounded, and fail-closed

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Platform clipboard services (Clipboard C0)
- **Supersedes:** None
- **Superseded by:** None

## Context

A clipboard history accumulates exactly the material users are least likely
to intend for retention: passwords routed through password-manager hints,
one-time tokens, and ordinary pastes that were never meant to outlive the
selection. No selected upstream daemon provides QindaQt's authenticated
lock/privacy and applet contract, so QindaQt will host a history — but the
default-safe shape of that history must be decided before any transport
exists, because retrofitting fail-closed behavior onto a stored-history
format is a privacy incident, not a refactor. See
[Clipboard service](../architecture/clipboard-service.md).

## Decision

1. The clipboard history is **volatile session memory**. No slice persists,
   syncs, or exports it; process exit, disable, and privacy loss all destroy
   it.
2. Storage is **allowlist-based**: canonical media types classify as storable
   only when explicitly listed; sensitive and one-time markers are refused;
   everything unknown is non-storable and refused. Refusal precedence is
   sensitive → one-time → non-storable.
3. Privacy denial (locked or lock-authority loss) and disabling purge the
   history and **raise a generation counter by exactly one**; every content
   mutation carries an expected generation and is refused on mismatch, and
   entry ids embed their generation so pre-purge handles cannot resolve.
4. The model is **pure**: Qt Core only, no QObject, no IPC, no Wayland, no
   clock — caller-supplied ticks are metadata, payload bytes leave the model
   only via an explicit promote, and value/descriptor codecs with a shared
   hostile-input decode floor are the single seam the future
   `ext-data-control-v1` adapter and Clipboard1 snapshot compose.
5. C0 connects to nothing: no host clipboard, no compositor, no bus, no UI.
   The live adapter and presentation are later reviewed slices.

## Consequences

- A hostile or compromised producer can pollute nothing: every dimension is
  bounded, unknown media fails closed, and refusals never mutate state.
- The future C1 host inherits deterministic, fully unit-tested semantics and
  only adds transport, FD lifetime, and authenticated lock-state authority.
- No cross-process history can exist until a transport ADR extends this one;
  the current codecs are bounded inline/descriptor forms, not a wire
  protocol.
- Presentation may show the bounded preview from snapshots without payload
  authority; full content requires the explicit promote path.

## Revisit when

- `ext-data-control-v1` leaves staging with semantics that change ownership
  or loss behavior, requiring a transport-side ADR.
- A durable multi-device or search-across-sessions clipboard outcome is
  accepted, which would supersede the volatility decision explicitly.
- The storable allowlist needs a new family; that is a documented policy
  change to the classification tables, not a code-only edit.
