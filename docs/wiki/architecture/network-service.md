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
and wire contract is fixed in [Network1 version 1](../reference/network1-v1.md)
and [ADR-0045](../adr/0045-fence-network1-pure-boundary.md).

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
  structurally secret-free. A `network_protocol` redaction helper is the single
  secret-recognition authority for diagnostics and bounded parameter maps; it
  covers quoted, unquoted, suffix-shaped, and malformed credential fragments.
  Unsafe or unredactable diagnostics fail closed to fixed public text, and the
  client refuses a credential-shaped parameter map before transport.
- A known-network id is the SHA-256 digest of the raw SSID octets plus the
  security suite, so intent exchange uses a stable pseudonym rather than a
  credential or the SSID of a hidden network. The digest is not a
  confidentiality boundary for guessable SSIDs.
- Device identity is the normalized interface name only; no MAC, driver, or
  persistent system identifier is a public value.
- Invalid UTF-8 and presentation-dangerous SSIDs become hidden networks rather
  than spoofable text. Validation rejects controls, line/paragraph separators,
  format controls (including bidi overrides and zero-width controls),
  surrogates, private-use, and unassigned scalars while accepting safe
  supplementary Unicode.

## Lineage and atomicity

The snapshot gate accepts a first snapshot with nonzero epoch/revision, demands
strictly increasing revisions within one owner and epoch, and demands an epoch
strictly greater than the retained accepted high-water on any owner or epoch
change. The high-water survives owner loss and current-model clear. A
same-owner epoch change is rejected as a forged restart; an A/B/A replay of a
replaced owner's epoch is rejected regardless of content. The model publishes a
candidate only after validation and gating pass; a rejected candidate leaves
every observable field untouched.

## Scan leases

A lease is the bounded right to keep one scan result set current. It is granted
only by an accepted snapshot, carries its granting epoch/revision, and its
published remaining duration is bounded between one second and two minutes.
Only after atomic snapshot admission does the consumer convert that duration
to an injected local monotonic deadline; invalid clocks and overflow are
rejected. While a live lease is held a second scan intent is refused as busy;
an expired lease no longer pins results. A lease from a foreign epoch can never
be adopted, so a stale snapshot cannot resurrect scan truth after an owner
change.

## Client truth

The client reports `Unavailable`, `Connecting`, `Ready`, or `Degraded` and
never shows unconfirmed data as current. Requests carry tokens and the exact
owner; the decoded snapshot owner must exactly equal both that request owner
and the current transport owner. Late, duplicated, foreign-owner, or
retired-token replies are dropped. Owner replacement or bus loss clears current
state while retaining the lineage high-water and reports unavailability. A
failed transport start rolls back every live flag, timer, request, operation,
owner, and public Ready state before a retry calls start again. A mutation that
times out, fails in transport, or returns a malformed or lineage-mismatched
reply is reported uncertain exactly once and is never automatically replayed;
the client refetches the authoritative snapshot instead of manufacturing state.

## Current N0 proof

The focused Debug and Release boundary consists of thirteen registered rows:
identity, validation, codec, redaction, snapshot gate, scan lease, intent
policy, model, client, adversarial hostile input, isolated installed consumer,
clean source boundary, and source-policy poison. The adversarial row pins the
exact-owner payload check, A→B→A retirement, maximum-integer lease rejection,
diagnostic cap, quoted credential redaction, bidi SSID rejection,
`wireValid=false` rejection, and failed-start rollback. The poison row must
prove the checker rejects QtDBus, `QTimer`, and NetworkManager tokens; merely
passing the clean source scan is insufficient.

## Remaining boundaries

Resident service ownership, the NetworkManager-backed adapter behind the
`NetworkTransport` seam, secret-agent interaction, persistence, Settings UI,
and physical radio/hardware qualification are later N1+ slices. N0 claims no
executable end-to-end connectivity.
