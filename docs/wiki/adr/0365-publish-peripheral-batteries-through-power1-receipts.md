# ADR-0365: Publish peripheral batteries through Power1 receipts

- **Status:** Accepted
- **Date:** 2026-10-08
- **Owners:** Power protocol, service, client and shell Power applet
- **Supersedes:** None
- **Superseded by:** None

## Context

UPower exposes mice, controllers, speakers and other peripheral batteries.
The existing Power1 snapshot correctly admits only system power supplies;
adding peripherals to that snapshot would alter its closed v1 wire and could
contaminate laptop aggregation and critical battery policy. The user requires
these exposed batteries in the Power popup.

Qt 6.11 method replies may expose an empty `QDBusMessage::service()`. Capturing
a requested owner is not proof of a reply sender. Power1 already has an
actual-sender nonce receipt boundary in [ADR-0316](0316-power-idle-state-receipt-authority.md).

## Decision

Extend the existing Power1 owner and object with a separate read-only domain:
`RequestPeripheralSnapshotWithReceipt(s nonce) -> b dispatched`, targeted
`PeripheralSnapshotReceipt(s nonce, ay payload)`, and
`PeripheralsChanged(t epoch, t revision)`. The boolean acknowledges dispatch
only. UI publication requires a genuine SignalMessage whose unique sender,
path, interface, member and signature match, plus the one-use pending nonce,
request token and current borrowed PowerClient owner/epoch. Subscribe before
requesting. Stop, owner/epoch loss and timeout consume the pending authority;
late, replayed and foreign signals cannot republish rows. No second owner
watcher, native bus receiver or shell UPower access is introduced.

The schema1 canonical payload has its own magic, bounded UTF-8 strings and
independent 64-row limit. Lengths are checked before allocation. Duplicate IDs,
unknown local enums, noncanonical unknown values, invalid numeric values,
trailing data and malformed UTF-8 reject the whole peripheral snapshot.
A visible omission count and truncation flag distinguish incomplete inventory.
The service epoch matches Power1; peripheral revisions are independent.
Battery authority loss clears rows; unrelated upstream epoch changes restamp
retained battery-authority facts. Collaborator generation fences reject stale
facts. No old Power1 field, capability, supply bound or aggregate changes.

The UPower adapter decodes peripheral facts independently from supply facts.
Only non-system-supply devices qualify; future upstream kinds map to Other.
Opaque IDs and sanitized model/vendor names cross the boundary; raw paths,
serials and hardware addresses do not. Exposed exact percentage, coarse level,
charge state and reported time estimates are retained. Unknown values stay
unknown; no estimate is inferred from a coarse level. Malformed peripheral
facts degrade this domain without revoking system battery controls.

Shell composition owns a sibling public PeripheralClient and transport,
borrowing the existing PowerClient authority. The applet receives copied rows
through its private facade and uses a bounded scrolling popup. All objects
share one Qt thread; dependencies outlive borrowers. External publication is
a reentrancy boundary: consume pending state first and guard any later access
with QPointer and current lineage checks.

## Consequences and verification

Old services time out only this optional domain; laptop battery and controls
continue through the unchanged PowerClient. Device appearance/removal refreshes
only observed inventory. The feature displays only batteries UPower exposes;
unsupported hardware cannot acquire invented values.

Focused tests cover canonical round trips and hostile lengths, malformed
peripheral isolation, speaker/controller/coarse/unknown decoding, generation
and epoch loss, and genuine private-bus foreign/replayed/late receipts with
deletion during public delivery. Installed laptop observation remains a
separate delivery gate; source tests do not prove physical hardware coverage.

See [Power architecture](../architecture/power-service.md) and
[Power1 reference](../reference/power1-v1.md).
