# ADR-0045: Fence the pure Network1 owner, lineage, lease, and secret boundary

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Network platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

Network1 will eventually cross a hostile process boundary while NetworkManager
remains the platform authority and credentials remain owned by a separate
secret agent. N0 establishes the reusable value, model, and client boundary
without claiming a resident service or platform integration. Owner confusion,
lineage replay, non-portable lease clocks, unbounded values, unsafe display
text, and public diagnostics that retain credentials would each make a later
transport unsafe even if that transport were otherwise correct.

## Decision

N0 is split into three focused libraries installed by one narrow component:

- `network_protocol` owns bounded public values, identity normalization,
  fail-closed validation and redaction, and the canonical codec;
- `network_model` owns the accepted lineage high-water, scan-lease truth,
  intent admission, and atomic consumer projection; and
- `network_client` owns asynchronous exact-owner binding, retry and timeout
  policy, uncertain outcomes, and an injected transport interface.

The decoded snapshot owner must exactly equal both the request owner and the
current transport owner. The model retains the highest accepted epoch across
owner loss, so A→B→A cannot revive a retired epoch. Snapshots carry a bounded
one-to-120-second remaining lease duration; only an admitted consumer converts
that duration to its injected local monotonic deadline, with overflow rejected.

All public strings and collections have protocol caps. SSID presentation
rejects unsafe Unicode categories, diagnostics use one canonical secret
redactor and fail closed, boolean bytes are canonical, and `wireValid=false`
is never admissible. A failed transport start is rolled back completely before
a bounded retry can start a fresh generation.

N0 contains no D-Bus connection, NetworkManager integration, platform radio
access, credential acquisition, persistence, or UI. A registered source-policy
test and deliberate poison fixtures enforce that negative boundary. Focused
installation uses the `QindaQtNetworkN0` component so a clean consumer can use
the three libraries without coupling to unrelated whole-tree artifacts.

## Consequences

- The last accepted snapshot may remain available internally for monotonic
  fencing, but it cannot be projected as Ready after authority loss.
- Future transports and adapters must enter through `NetworkTransport` and
  satisfy the exact owner, lineage, caps, and secret-free contracts before
  publication.
- Known-network ids are stable correlation pseudonyms, not a confidentiality
  mechanism; callers must not present their derivation as hiding a guessable
  SSID.
- N1 must add the resident service and platform adapter without weakening this
  pure boundary or moving platform objects into these modules.

## Revisit criteria

Revisit with a superseding ADR before changing the owner/lineage authority,
lease representation, public secret policy, canonical wire layout, or the
three-module dependency direction.
