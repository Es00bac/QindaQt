# Native keyring storage

PK1 provides `QindaQt::KeyringStorage`, a transport-free C++20 static library
under `src/services/keyring`. It owns one collection's staged item records,
locked memory, OpenSSL encryption, bounded binary format and atomic persistence.
The program plan `docs/plans/2026-09-28-plasma-free-qindaqt.md` defines the
later service, PAM, prompt, Settings, portal and import slices.
[ADR-0292](../adr/0292-native-keyring-storage.md) records security choices and
limits. The [module boundaries](module-boundaries.md) govern its public API.

## Public ownership and behavior

`CollectionStore` accepts an explicit absolute fixture or production directory,
a safe ASCII collection ID (1–60 bytes) and optional synchronous commit
cancellation callback that must not reenter storage. A filename is `<id>.qkr`; the library does not choose
a user path, contact another service or install anything. The parent must
already exist; a missing final 0700 directory can be created.

All calls and borrowed pointers are confined to one caller thread. Item records
own move-only `SecureBuffer` secrets. Metadata consists of label, MIME content
type, caller identity supplied by the future daemon, created/modified timestamps
and non-secret attributes. The caller must never encode a password in metadata.
Borrowed item/secret pointers expire on mutation, lock, load, unlock, rekey and
destruction. Password spans are borrowed for the duration of one operation;
the caller owns wiping its input. Internal password copies are never made by
the module; OpenSSL provider behavior remains outside secure-buffer ownership.

- `create` stages an empty unlocked collection with fresh salt and search key;
  `save` refuses to replace an existing file for that new collection.
- `load` retires prior decrypted state and loads an unauthenticated locked
  snapshot. `unlock` first locks, derives, authenticates and decodes into
  temporary secure allocations; only full success publishes decrypted items.
- `put` consumes a record even on rejection. Bounds are checked before
  accepting it. `erase` and `save` require an unlocked collection.
- `lock` wipes keys and item secret allocations and discards unsaved changes.
  Locked searches and subsequent unlock use the last sealed snapshot; callers
  must save before lock when they intend to retain mutations.
- `rekey` saves with a new salt and password-derived key. A pre-rename failure
  restores the old sealed envelope/key; an unexpected staging failure locks.
- `search` matches all supplied attribute pairs. An empty query returns every
  indexed ID. Invalid query bounds return no matches. A freshly loaded index or
  failed unlock yields `authenticated=false`; verified in-process snapshots
  remain verified after lock. Search never returns a secret.

Errors are sanitized enums: invalid input/format, authentication failure,
locked, unavailable secure memory/crypto, I/O failure, or durability unknown.
They contain no password, secret or record text. Ordinary allocation exhaustion
may throw `std::bad_alloc`; consumers must fail the request closed.
`SecureBuffer` construction throws a sanitized runtime error if mmap, mlock or
MADV_DONTDUMP/DONTFORK fails. Fork children see inherited buffers as empty and do not inherit their secret pages.
It has no copy or text conversion; move transfers ownership,
`wipe` zeros all pages, and `clear` wipes before munlock/unmap. Empty secrets
are valid.

## Format version 1

All integers use unsigned big-endian encoding, without alignment padding.
Format v1 fixes Argon2id version 0x13 (1.3), a 32-byte derived key, and a 16-byte GCM tag.

| Offset | Field |
| --- | --- |
| 0 | 8-byte magic/version `QKEYR001` |
| 8 | u32 Argon2id memory KiB |
| 12 | u32 iterations |
| 16 | u32 lanes |
| 20 | 16-byte salt |
| 36 | 12-byte GCM nonce |
| 48 | 32-byte independent public search key |
| 80 | u32 index byte length |
| 84 | u32 ciphertext byte length |
| 88 | Index, then ciphertext, then 16-byte GCM tag |

The index begins with u32 item count; each item is length-prefixed ID, u32 digest
count and that many 32-byte HMAC-SHA256 digests. IDs and digests are strictly
sorted and unique. HMAC input is u32 name length, name bytes, u32 value length,
value bytes. The AAD is exactly the fixed header plus serialized index.

The encrypted payload is u32 item count, then each item's length-prefixed ID,
label, content type and creator, u64 created and modified times, u32 attribute
count, length-prefixed name/value pairs, and u32 secret length plus secret bytes.
Exactly the declared bytes must be consumed. The decoded index must match the
authenticated public index before publication.

Limits: 8 MiB file, 4 MiB payload, 1024 items, 32 attributes/item, 64-byte item
IDs, 1024-byte attribute names/values and labels/creator, 128-byte MIME type,
1 MiB secret and 4096-byte password. Argon2 defaults/bounds are in ADR-0292.
Declared lengths, counts and resource parameters are validated before expensive
KDF work. A cipher payload's secret records remain in locked pages, never JSON,
QByteArray or ordinary vector plaintext.

## Search and threat boundary

The independent search key is intentionally public to a file reader. HMAC
digests avoid raw attribute strings but permit offline dictionary guesses;
they do not promise attribute confidentiality. Count, identifier, attribute
count, ciphertext length and equality leakage also remains. Before unlock,
an attacker who can modify the file can forge query matches; the explicit
authentication flag conveys that uncertainty. Reading a secret always requires
successful authenticated unlock.

No encryption key is persisted or retained after lock. Process core suppression
and caller password handling belong to the future daemon/PAM boundary. This
module's application buffers use locked, dump-excluded, wiped pages; OpenSSL
internal allocations are not covered by those guarantees. Same-user compromise,
root and replay of an old authentic snapshot are outside this core's protection.
PK2 must enforce one writer, rate-limit/serialize KDF work, and disable core dumps.

## Persistence

Traverse all directory components through no-follow directory descriptors;
reject a symlink, unsafe final mode/ownership, insecure or hard-linked file and
nonregular file. Preserve old bytes on any failure before rename, including a
false/throwing commit callback. Temporary files are exclusive 0600, fsynced,
renamed, then the directory is fsynced. A new collection never overwrites an
existing file. If parent fsync fails after rename, `DurabilityUnknown` explicitly
means the new bytes may already be visible: reload before presenting a saved
result. This is not a cross-process writer lock.

## Qualification

Focused synthetic tests cover default/custom encrypted round trips, metadata,
empty secrets, subset search while locked, the authentication trust flag,
wrong passwords retiring earlier state, fresh nonces, rekey, KDF/size/count
hostile headers, every truncation, version/trailing bytes, bit flips in header,
index, cipher and tag, authenticated malformed payload/index disagreement, pre-rename cancellation preserving old data/rekey,
permissions, symlinks, hard links, FIFO/oversize rejection and memory wipe/move,
locked-page/core/fork-exclusion evidence and unavailable-mlock failure.

```sh
cmake --preset dev
nice -n 12 ionice -c3 cmake --build build/dev --target tst_keyring_secure_buffer tst_keyring_collection_store tst_keyring_storage_failures tst_keyring_storage_decoding -- -j4 -l16
ctest --test-dir build/dev -R '^keyring_' --output-on-failure
./tools/validate-docs
mkdocs build --strict
```

These tests do not read user keyrings/wallets, own a secrets name, activate
PAM/daemon/UI, import real data, or prove live-service compatibility. Those
remain the next slices. See the [testing harness](../development/testing-harness.md)
for later isolated service qualification.

The [native daemon](keyring-daemon.md) supplies sole-writer, alias, prompt and process policy through this public storage boundary.

## Atomic insertion batch

`insertBatchAndSave(vector<Item>)` consumes additions and rejects empty batches,
existing/duplicate IDs, individual bounds or aggregate limits. It stages a deep
secure copy of the current collection and inserts only into that candidate.
One durable save replaces the file before in-memory publication; pre-rename
failure preserves original decrypted memory, borrowed pointers and disk bytes.
Success invalidates borrowed pointers. DurabilityUnknown reloads locked and
exposes no candidate pages; the caller must explicitly recover rather than
report saved/rolled back. Existing commit cancellation is readonly/non-reentrant.
This generic storage operation chooses no app/import conflict policy or paths;
[ADR-0312](../adr/0312-preserve-exact-legacy-portal-secrets.md) supplies the separate
portal policy. Format1 and existing save/rekey operations remain compatible.

## One-time collection import transaction

`QindaQt::KeyringImportStore` owns `CollectionImportCatalog`, a synchronous,
thread-confined public DTO/receipt boundary. Its factory receives the native
absolute directory and holds the daemon's exclusive catalog writer lease.
Consumers supply owned item secrets, bounded native filename IDs, original
source IDs/labels/dates and aliases; they never edit the private catalog JSON.
`CollectionImportPasswords` returns owned SecureBuffer password pages or explicit
cancellation/failure. Credentials are never arguments or metadata. The borrowed
readonly admission callback joins independently accepted source/session lifetime
and must outlive the call without reentering the transaction.

The batch admits at most64 persistent collections, unique native IDs and unique
source-kind/source-ID pairs. Existing per-collection item, secret, attribute and
4 MiB payload limits still apply. Unsafe IDs, reserved volatile session ID,
invalid provenance, duplicate IDs and conflicting aliases fail the whole batch.
Aliases point only to collections in the accepted batch. Existing collections
must match source provenance, labels/dates and every authenticated item byte and
metadata field exactly after password authentication; they are then locked.
They are never saved or overwritten. Wrong passwords report AuthenticationFailed.
An exact retry with unchanged aliases writes nothing.

New collections are sealed to previously absent files, then locked. Only one
durable catalog publication exposes the complete batch and aliases. A failure
before catalog rename removes only newly staged unreferenced files and preserves
existing collections/catalog. The pre-rename admission check refuses source loss.
A crash leaves unlisted encrypted orphans, which the existing catalog loader
removes on restart. The factory initializes an empty catalog before staging, so
an absent catalog cannot accidentally admit such orphan files.

After catalog rename, failed directory fsync reports DurabilityUnknown with zero
success counts and reloads the published catalog in locked state. It never
claims rollback. A failed recovery disables further import on that adapter;
close and reopen only after resolving storage availability. Existing resident
create/delete behavior and catalog version1 remain compatible; entries optionally
carry non-secret import provenance (`kind`, `source`) in the owning private schema.
This is an owning one-time seam, not a resident untrusted bulk-mutation wire.
The complete legacy reader/CLI remains a separate delivery step in
[ADR-0312](../adr/0312-preserve-exact-legacy-portal-secrets.md).
