# Network service architecture

This page records the accepted shape of QindaQt network connectivity and the
current **N0 pure boundary** maturity for the Platform services milestone
(QQ-005.04) tracked in the
[implementation roadmap](../development/implementation-roadmap.md).
N0 owns bounded Network1 values, secret-free identity, canonical codecs, the
lineage-gated model, scan-lease truth, intent admission, and an asynchronous
client over an injected transport seam. N0 deliberately ships no transport
implementation: no NetworkManager, BlueZ, D-Bus, socket, real radio, secret
store, or user interface exists in this slice, and the
`qindaqt.network-boundary` test fails if one appears. The value, validation,
and wire contract is fixed in [Network1 version 1](../reference/network1-v1.md).

## Authority map

| Concern | Truth authority | QindaQt owner |
| --- | --- | --- |
| Connectivity, radios, devices, scans | Future NetworkManager adapter | Resident `Network1` service (N1+, not N0) |
| Known-network credentials | Secret agent outside the model | Never transported through Network1 values |
| Radio hardware/software state | Adapter observation only | N0 models intent and observed truth; no mutation path is claimed |
| Scan result freshness | Snapshot-published lease | `network_model` lease tracker |
| Lineage and stale rejection | Snapshot gate | `network_model` gate, one authority for N0 |

## Modules

| Module | Cohesive responsibility |
| --- | --- |
| `network_protocol` | Bounded values, SSID/BSSID/interface identity, derived known-network ids, validation, secret redaction, canonical byte codecs |
| `network_model` | Snapshot lineage gate, scan-lease reconciliation, intent admission, atomic consumer projection |
| `network_client` | Asynchronous exact-owner client, bounded deadlines and retries, uncertain-outcome reporting, and the injected `NetworkTransport` seam |

The pure model constructs no timer, file, process, or Qt object; time enters
only through an injected monotonic clock so tests are deterministic. The client
owns nothing beyond its `QTimer`s and is thread-confined to the Qt main loop.

## Security and least authority

- Access points, known networks, intents, and operation parameters are
  structurally secret-free; a `network_protocol` redaction helper is the single
  secret-recognition authority for diagnostics and parameter maps, and the
  client refuses a credential-shaped parameter map before transport.
- A known-network id is the SHA-256 digest of the raw SSID octets plus the
  security suite, so UI and intent exchange opaque ids, never credentials or
  the SSID of a hidden network.
- Device identity is the normalized interface name only; no MAC, driver, or
  persistent system identifier is a public value.
- Non-printable or non-UTF-8 SSIDs become hidden networks rather than
  spoofable presentation text.

## Lineage and atomicity

The snapshot gate accepts a first snapshot with nonzero epoch/revision, demands
strictly increasing revisions within one owner and epoch, and demands an epoch
strictly greater than every observed epoch on any owner or epoch change. A
same-owner epoch change is rejected as a forged restart; an A/B/A replay of a
replaced owner's epoch is rejected regardless of content. The model publishes a
candidate only after validation and gating pass; a rejected candidate leaves
every observable field untouched.

## Scan leases

A lease is the bounded right to keep one scan result set current. It is granted
only by an accepted snapshot, carries its granting epoch/revision, and its
deadline is bounded between one second and two minutes. While a live lease is
held a second scan intent is refused as busy; an expired lease no longer pins
results. A lease from a foreign epoch can never be adopted, so a stale snapshot
cannot resurrect scan truth after an owner change.

## Client truth

The client reports `Unavailable`, `Connecting`, `Ready`, or `Degraded` and
never shows unconfirmed data as current. Requests carry tokens and the exact
owner; late, duplicated, foreign-owner, or retired-token replies are dropped.
Owner replacement or bus loss clears the whole lineage and reports
unavailability. A mutation that times out, fails in transport, or returns a
malformed or lineage-mismatched reply is reported uncertain exactly once and is
never automatically replayed; the client refetches the authoritative snapshot
instead of manufacturing state.

## Remaining boundaries

Resident service ownership, the NetworkManager-backed adapter behind the
`NetworkTransport` seam, secret-agent interaction, persistence, Settings UI,
and physical radio/hardware qualification are later N1+ slices. N0 claims no
executable end-to-end connectivity.
