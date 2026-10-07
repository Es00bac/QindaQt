# Removable Media Devices protocol version 1

This page specifies the standalone `QindaQt::RemovableMediaProtocol` value and
canonical codec boundary from [ADR-0350](../adr/0350-share-removable-media-with-file-consumers.md).
The codec is implemented separately from the
[public client and owner Devices exporter](../architecture/removable-media-client.md).
No installed service currently exports this new wire, and no File Manager/chooser
device UI is delivered by this module.
[Removable Media](../apps/removable-media.md) still owns private device policy,
persistence, credentials and UDisks operations.

## Public values and authority

The owning `QString`, `QStringList`, `QList` and `std::optional` values may be
copied between threads; shared mutable instances require caller synchronization.
No value contains a QObject, borrowed storage, connection, private device path,
secret, force option or executable. Codecs and structural validators are
reentrant and never access a bus/filesystem. Parsing a path, display id or
attachment handle cannot grant authority, prove a real mount or certify safe
removal. Future clients/exporters must still enforce exact-owner lifecycle and
current backend admission.

Constants name service `org.qindaqt.RemovableMedia1`, object
`/org/qindaqt/RemovableMedia1/Devices`, interface
`org.qindaqt.RemovableMedia1.Devices` and version 1. Existing Activate remains
unchanged; the constants alone do not register any object or activate a helper.

| Value | Ordered fields |
| --- | --- |
| Lineage | owner text, epoch text, revision u64 |
| Attachment | handle text, generation u64 |
| Diagnostic | code u32, message text |
| ActionAvailability | enabled bool, disabled reason u32 |
| VolumeRow | driveDisplayId text, volumeDisplayId text, displayName text, kind text, partitionNumber u32, sizeBytes u64, Attachment, mountRoots text list, preferredRoot text, mountState u32, readOnly u32, encrypted/locked/optical bools, six ActionAvailability values in open/mount/mountReadOnly/unmount/remove/showDetails order, progress u32, outcome u32, Diagnostic |
| ActionRequest | protocolVersion u32, requestId text, Lineage, Attachment, action u32 |
| PendingOperation | operationId text, ActionRequest, phase u32 |
| Snapshot | protocolVersion u32, Lineage, availability u32, VolumeRow list, optional PendingOperation, Diagnostic |
| ActionAdmission | ActionRequest, status u32, operationId text, Diagnostic |
| OperationResult | ActionRequest, operationId text, status u32, Diagnostic, optional confirmingRevision u64, removalMode u32 |

Zero partitionNumber means unknown/nonpartition. Size and generation values
are unsigned and never narrowed. Display ids remain presentation-only;
attachment generation and bound lineage are correlation, not authorization.

## Canonical binary envelope

All integers are fixed-width little endian. Every message begins with four
literal bytes `QRMD`, codec-version u32 `1` and message-kind u32. Kinds are
Snapshot=1, ActionRequest=2, ActionAdmission=3 and OperationResult=4. Its body
then follows the ordered table above. The body protocolVersion is 1 (for
admission/result it is in their nested ActionRequest). Unknown codec/body
versions and wrong kinds are refused; decoders never guess another layout.

Text is u32 UTF-8 byte length followed by exactly those bytes. Lists are u32
count followed by exactly that many values. Optional values are bool presence
followed by their body only when present. A Boolean is exactly one byte 0 or 1.
No native-width integers, QDataStream implicit arrays, QVariant maps, padding,
terminators or trailing bytes occur. List order is part of the value; the
codec preserves it rather than making an application ordering decision.

UTF-8 must be valid and canonical, including complete final multibyte sequences;
NUL is refused. Literal leading U+FEFF and U+FFFD are preserved as data, with
no BOM removal or silent replacement. Encode validation refuses invalid UTF-16
source strings such as unpaired surrogates. Successful decode/re-encode
therefore reproduces the input bytes exactly.

## Shared bounds

| Bound | Limit |
| --- | --- |
| Distinct visible drive ids | 32 |
| Volume rows | 128 |
| Mount roots per volume | 8 |
| Complete snapshot bytes | 256 KiB |
| Complete request/admission/result bytes | 16 KiB |
| Absolute mount root UTF-8 bytes | 4096 |
| Display name/kind UTF-8 bytes | 256 each |
| Diagnostic message UTF-8 bytes | 1024 |
| Opaque identifier ASCII bytes | 64 |
| Unique-owner bytes | 255 |

Opaque identifiers are nonempty, case-sensitive `[A-Za-z0-9_-]` strings;
owner names use the D-Bus unique-name lexical form with colon and at least two
nonempty dot-separated ASCII components. Lexical validation does not attest a
real owner. An entirely unbound lineage (empty owner/epoch and revision zero)
is allowed only in loading/unavailable snapshots with no rows or pending work.
Ready snapshots, requests, admissions and results require fully bound lineage
and nonzero revision/generation. No partially bound lineage is accepted.

Repeated driveDisplayId across partitions and repeated display names are
valid. Duplicate volumeDisplayId, attachment handle or a volume's mount root
fails the complete value. Mount roots are literal normalized absolute local
paths: no URI, double slash, dot/dot-dot component, trailing slash except `/`,
NUL or oversized text. Mounted rows require at least one root and preferredRoot
in that list; unknown/unmounted rows have no roots. Locked rows must be encrypted
and cannot claim a mounted root. ActionAvailability is enabled exactly when
its disabled reason is None. These consistency checks do not calculate policy.

Loading/unavailable snapshots cannot carry ready rows/pending work. A pending
operation requires an id, non-idle phase and initiating owner/epoch equal to
the snapshot's, with revision not newer than the snapshot. Its row may have
already disappeared during authoritative removal; this preserves correlation
without reviving a revoked handle.

Admission has an operation id exactly for Accepted. Terminal result status
cannot be None. Applied mount/mount-read-only/unmount requires a confirming
revision strictly newer than the initiating revision; other statuses cannot
claim confirmation. Applied Remove has exactly one final non-None removal
mode; other actions/statuses have None. These are protocol shapes for future
owner facts, not physical completion assertions by the codec.

## Closed enums

| Type | Consecutive wire values starting at zero |
| --- | --- |
| Availability | Loading, Ready, Unavailable |
| MountState | Unknown, Unmounted, Mounted |
| ReadOnlyState | Unknown, ReadOnly, Writable |
| Action | Mount, MountReadOnly, Unmount, Remove, ShowDetails |
| ProgressPhase | Idle, Confirming, Mounting, Unmounting, Locking, Ejecting, PoweringOff, Refreshing |
| OperationStatus | None, Applied, Refused, Cancelled, Busy, Gone, Uncertain |
| RemovalMode | None, Unmounted, Ejected, PoweredOff |
| AdmissionStatus | Accepted, Unsupported, Unavailable, Stale, Gone, Busy, NotAdmitted, Invalid |
| DiagnosticCode | None, Unsupported, Unavailable, Stale, Gone, Busy, NotAdmitted, Invalid, Cancelled, Uncertain |
| DisabledReason | None, Unsupported, Unavailable, Stale, Gone, Busy, Locked, ReadOnly, NotMounted, AlreadyMounted, NotAdmitted |

No enum includes format, secret unlock, forced unmount, preference write or
arbitrary command. A new wire enum/layout needs an accepted version change;
unknown ordinals fail closed.

## Failure and verification contract

Encode validates before writing and returns an empty payload on every failure.
Decode checks the whole-envelope cap before interpreting it, then field/count
caps and remaining input before text allocation. It builds owning temporary
values, validates complete structure and only then replaces the destination.
Every failure leaves the destination unchanged and returns typed CodecError
plus ValueError when structural validation failed; it returns no raw hostile
content in an error. Future client publication must independently retire
untrusted snapshots on malformed/owner-loss events rather than treating an
unchanged destination as retained ready truth.

Focused pure tests are `qindaqt.removable-media-protocol-codec`, `hostile` and
`validation` (each with the full prefix). They check independent byte vectors,
Unicode preservation, owning roundtrips, maximum scalar/collection/envelope
boundaries, every truncated prefix, wrong kinds/versions/enums/Boolean bytes,
invalid text, duplicate identities, forged allocation lengths/counts,
normalized roots and authoritative-result shapes. They require no display,
private bus, device, credentials or media preferences. The owning `tests/services/removable_media_protocol/standalone` CMake entry point runs these gates without unrelated desktop dependencies. Its `installed_consumer` sibling links only the staged public header directory/archive and Qt Core; a missing staged header must fail compilation.

The module installs its static library and public header file set through the
existing `QindaQtTargets` export set. Public client transport, read-only owner
exporter, installed client consumer and Devices runtime qualification are the
next boundary; File Manager and chooser follow separately. ED-04 remains open.
