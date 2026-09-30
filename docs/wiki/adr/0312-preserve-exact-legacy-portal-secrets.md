# ADR-0312: Import complete legacy stores and preserve exact portal secrets

- Status: Accepted
- Date: 2026-09-30
- Related: [ADR-0310](0310-native-per-application-secret-portal.md), [ADR-0292](0292-native-keyring-storage.md)

## Context

The current KWallet Secret backend stores per-app opaque64-byte records.
Applications may encrypt existing data with those exact bytes; generating or
truncating them would break that data. A partially published import or silent
replacement of an existing native app record is also unacceptable.

The full PK6 requirement copies every legacy collection/item before the native
secrets-name switch. Portal records are only one subset; collection provenance,
source acquisition and catalog publication need separate owning boundaries.

## Decision

The initial PK6 slice provides a bounded synthetic acquisition contract, pure import planner and
separate public-store persistence collaborator. Records carry original
case-sensitive nonempty app ID, exact64 SecureBuffer bytes and bounded source
wallet identity. Stored provenance fixes schema kwallet-secret-portal-v1 and
folder xdg-desktop-portal, with record version legacy-opaque-64. The planner
neither opens wallets nor authenticates caller-supplied provenance: an eventual
authorized acquisition boundary owns that responsibility.

At most128 records enter one plan. Malformed inputs, duplicate app IDs or any
existing nonidentical record fail the entire batch. Exact bytes and identical
provenance are idempotent. Fresh-native-32, unrelated records and changed legacy
bytes/provenance are conflicts and are never overwritten. There is no implicit
merge, regeneration, KDF or truncation.

The generic owning CollectionStore insertBatchAndSave stages the entire current
collection with separate secure pages and rejects duplicate/existing IDs. One
atomic durable save publishes all additions. Pre-rename failure leaves original
memory and disk intact. DurabilityUnknown reloads locked, reports no additions
and requires explicit caller recovery; it cannot claim rollback after rename.
The format and existing single-record save/rekey behavior remain compatible.

Only strict native32 or legacy64 records can be retrieved by the owning daemon;
the same authenticated portal/nonce/native-Unlocked boundary returns exact bytes.
The FD writer bounds either length, never normalizes the secret. Synthetic
fixtures seed public storage before the daemon's exclusive writer starts and
verify restart/frontend behavior. No resident runtime import method, live import, PAM change or installation is introduced by that slice.

## Consequences

Legacy records with empty/invalid IDs or lengths other than64 are explicitly
unsupported. Stored source metadata is non-secret and encrypted/authenticated
with the existing collection. Same-user unlocked Secret Service access remains
the existing sharing boundary. Actual wallet acquisition, authentication,
operator approval and installation are separate manager gates.

See [native Secret portal](../architecture/secret-portal.md) and
[native keyring storage](../architecture/keyring-storage.md) for the public
ownership/failure contracts and executable private acceptance evidence.

Native portal prompts publish targeted `PortalPromptResult(s nonce, o prompt,
u response, ay secret)` receipts bound to the original acquisition nonce,
exact retained daemon owner, actual signal sender and owned prompt path. Success
requires a fresh final authenticated native Unlocked policy receipt. Response 1
means cancellation; response 2 means a record/persistence failure. Forged, stale
or duplicate receipts grant no bytes. Standard `Prompt.Completed(b,v)` remains
wire compatible for standard Secret Service clients.

## Full collection acquisition boundary

The normative PK6 outcome also copies every Secret Service collection/item and
KWallet entry through one-time compatibility readers before a secrets-name
switch. Opaque64 portal records are only one subset. Original provider files are
not edited by the importer; acquiring real wallets and final deployment remain
separate manager gates. The public owning collection transaction is implemented
as documented in [keyring storage](../architecture/keyring-storage.md): staged
new encrypted files, one catalog publication, exact-existing idempotence,
whole-batch conflicts and locked recovery on durability uncertainty. Source IDs,
labels/dates and aliases remain distinct from bounded native filename IDs.

Legacy providers cannot implement the native nonce receipts. Their confined
reader therefore uses libdbus-1's primary `dbus_message_get_sender` API to require
the pinned actual unique owner on every accepted snapshot/secret reply, with
source owner/PID/session lifetime fencing. That dependency belongs only to the
one-time reader executable and never to the resident daemon. Its DTO boundary
has no DBus internals. The [one-time importer](../architecture/keyring-import.md) now implements these
readers, complete snapshot mapping and native password/display/lock composition.
It preserves every KWallet raw entry/type/folder/key and each Secret Service
collection/item/label/date/attribute. Exact portal64 records are additionally
derived into fixed login, never substituted for the original wallet entries.
Known aliases are explicit because standard ReadAlias has no enumerator. Source
APIs have no atomic export, so bounded exact dual passes and data-change signal
retirement require operator quiescence; no linearizable export claim is made.

The catalog's separate cooperative checkpoint dispatches queued native lifetime
retirement between staging steps and before publication, distinct from readonly
admission. Dependencies remain borrowed/same-thread and nested commit is refused;
atomic rename dispatches no events. Default passwords use the owned native helper;
optional trusted anonymous FD frames bound each password/deadline and never use
argv/text. Process core/dump hardening precedes reads; owning secret pages wipe,
but temporary legacy/framework wire allocations are not universally locked or
wiped. The private provider matrix qualifies synthetic acquisition/sealing and
rollback/idempotence only. Real user-data acquisition, installed native UI/helper
journeys, name switch and Portage/overlay retirement remain manager delivery gates.
