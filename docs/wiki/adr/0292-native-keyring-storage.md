# ADR-0292: Own a bounded native keyring storage core

- Status: Accepted
- Date: 2026-09-29
- Decision owners: QindaQt program manager and keyring module
- Scope: PK1 storage; PK2–PK6 remain separate delivery boundaries

## Context

The owner requires a native QindaQt key store with Secret Service compatibility,
login unlock and eventual import. Storage must be usable before those processes
exist, and must not read existing user wallets during qualification. The
program plan `docs/plans/2026-09-28-plasma-free-qindaqt.md` defines the outcome.
A password-derived encryption key cannot support arbitrary attribute queries
after it has been wiped on lock.

## Decision

A transport-free C++20 static library owns collection values, secure memory,
bounded binary serialization, cryptography and Linux file replacement. No bus,
PAM, UI, environment-derived path or session side effect belongs to PK1.

Use OpenSSL 3 EVP exclusively: Argon2id derives a 32-byte encryption key from a
borrowed password and random 16-byte salt; AES-256-GCM uses a fresh random 12-byte
nonce per save and authenticates the whole header and index. Argon2id parameters
are versioned and validated before expensive work. OpenSSL 3.2 or newer is
required; unavailable providers fail closed. Defaults are 64 MiB, three passes,
one lane. Accepted bounds are 8–256 MiB, 1–8 passes and 1–4 lanes. Those upper
bounds bound one request, not aggregate daemon demand; PK2 must serialize and
rate-limit unlock attempts.

Persist an independent random 256-bit **public search key** alongside keyed
HMAC-SHA256 attribute-pair digests. Frame each pair with big-endian lengths.
This key never encrypts secrets. Lock destroys the encryption key and decrypted
item buffers; only ciphertext and public search metadata survive. Persisting
the independent key is a deliberate searchable-metadata tradeoff, **not
confidential attribute encryption**. A file reader can guess candidate
attributes offline, including dictionary attacks on low-entropy values. Item
IDs, counts, attribute counts, ciphertext length and equality are also visible.
Labels, content type, creator, dates and raw attributes are encrypted on disk,
but are ordinary non-secret metadata in application memory while unlocked.

The entire public index and search key are authenticated by GCM only on unlock.
A freshly loaded locked index therefore has no integrity proof:
`SearchResult.authenticated` is false. A successful unlock rebuilds and compares
the index against the encrypted item records before publishing any secret.
Lock after verified unlock keeps that in-process verification result for the
same snapshot; later load or failed unlock resets it. PK2 must expose uncertain
search results truthfully and must never authorize secret disclosure from them.

Secret application allocations use move-only dedicated anonymous pages,
`mlock`, `MADV_DONTDUMP`, `MADV_DONTFORK` and `OPENSSL_cleanse` of the complete mapping before
unlock/unmap. Any inability to lock or exclude pages from core dumps fails
allocation. Serialized/decrypted secret records use the same secure buffer;
encrypted bytes and non-secret metadata use ordinary containers. EVP/provider
internal allocations and stacks are controlled by OpenSSL, not this buffer;
the future resident and PAM bridge must additionally disable core dumps and
avoid ordinary password temporaries. This does not protect a compromised
same-user process, root, firmware, snapshots of RAM, or external caller copies.

Use an explicit absolute directory path, traverse every component with
`O_DIRECTORY|O_NOFOLLOW`, and require its final directory to be euid-owned
0700. Existing collection files must be regular, euid-owned, singly linked
0600. Never follow a symlink or repair an insecure user-owned directory silently.
Only the final directory may be created. Files use random exclusive temporary
names, 0600, file fsync, atomic rename, then directory fsync. New collections use
`RENAME_NOREPLACE`; a cancellation gate before rename leaves old bytes intact.
Failure after rename is explicitly `DurabilityUnknown`, requiring reload before
a caller reports persisted state.

## Consequences

- Storage is testable entirely with synthetic temporary fixtures and requires
  no active secrets bus, service unit, PAM configuration or installed binary.
- The binary format is bounded to 8 MiB files, 4 MiB decrypted payloads, 1024
  items, 32 attributes/item, 1024 bytes per attribute name/value, and 1 MiB
  per secret. Unknown versions, malformed lengths, ordering and KDF values fail
  before derivation. Available locked-memory limits can impose tighter bounds.
- Authenticated decryption, decode and index comparison complete before
  publishing the encryption key or items. Every failed unlock leaves secrets
  unavailable; failed loads also retire earlier decrypted state.
- Locks deliberately discard unsaved mutations and decrypted state. Callers
  must save before lock; they must reload the persisted snapshot after failed
  persistence if they want to discard staged metadata/index changes.
- Atomic file replacement is crash-consistent on filesystems honoring fsync.
  Newly created leaf directories also fsync their parent; power-loss durability
  still depends on the underlying filesystem and storage hardware.
- No rollback detection, file locking across writers, credential policy,
  multi-process arbitration, default alias policy or migration is implied.
  [PK2](0296-native-keyring-daemon-boundary.md) now supplies the sole-writer lock; replay of an older authentic file remains possible.
- PK2 owns process dump policy, Secret Service sessions and authenticated caller
  metadata; PK3 owns PAM borrowed-password lifetime. PK2 includes a minimal prompt seam/helper; PK4 extends full prompts and UI.
  Those boundaries must not access storage private headers.

See [Native keyring storage](../architecture/keyring-storage.md) for the wire
format, error and lifetime contract and executable acceptance gates.
