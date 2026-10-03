# One-time native keyring import

`qindaqt-keyring-import` copies every bounded collection/item exposed by a
selected running Secret Service and every KWallet wallet/entry through their
public D-Bus APIs. It seals native destination files before the manager switches
`org.freedesktop.secrets`. It never claims that name, activates a legacy provider,
reads legacy files directly, deletes originals, or supplies credentials in argv.
Actual user-data acquisition and Portage deployment remain separate authorized
delivery gates. Development and acceptance use private synthetic providers only.

## Components and ownership

`QindaQt::KeyringLegacyReader` is a one-time, same-thread compatibility reader.
Its public `LegacySourceBinding` and owned snapshot DTOs contain no libdbus
internals. The private transport uses installed libdbus-1's
[`dbus_message_get_sender`](https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html)
to validate the actual pinned unique owner on every accepted provider reply and
signal. Qt method replies cannot provide that evidence. The reader alone adds direct
`dbus-1` linkage (`sys-apps/dbus` for final Portage packaging); it adds no KDE or
GNOME library to the resident daemon or native runtime.

`QindaQt::KeyringImportPolicy` consumes snapshots through the pure
`planLegacyCollections` boundary. It chooses bounded native IDs, preserves all
entry bytes and metadata, and derives the legacy portal subset. It opens no
provider, storage, prompt or bus. `QindaQt::KeyringImportStore` owns the existing
[collection/catalog transaction](keyring-storage.md#one-time-collection-import-transaction),
exclusive destination writer and private catalog schema. The CLI composes these
public boundaries with ordinary display attachment, native lock state and the
keyring module's existing owned prompt provider.

Each source is independently selected by service, exact unique owner, expected
PID and accepted session unique owner. The reader joins actual bus-daemon owner,
UID and PID facts to a retained kernel PIDFD; source/session loss, replacement,
process death, lost native admission or expiry permanently retires it. This is
ordinary selected-session identity/lifetime, not executable attestation. Neither
process names nor shell commands supply application identity or permission.

## Complete source snapshots and compatibility

The [standard Secret Service API](https://specifications.freedesktop.org/secret-service/latest-single/)
reader enumerates Collections and each collection's Items, labels, dates and
attributes. It opens an owned `plain` session, unlocks locked collections/items
through actual-owner prompts, and accepts GetSecrets only from the retained
source owner with the requested paths and session. Cancellation, missing bytes,
malformed values or unavailable algorithms fail explicitly. Other algorithms
are not implemented by this compatibility reader. Standard session Close and
owned prompt Dismiss are best effort; disconnect releases the client session.

ReadAlias accepts a *named* alias and supplies no alias enumeration operation.
The tool therefore preserves `default` and every additional operator-known
`--alias`. It cannot discover arbitrary hidden aliases. A source collection named
`session` is copied to a distinct durable native ID; it never replaces the native
volatile session collection. Empty source collections are retained.

The [primary KWallet public interface](https://invent.kde.org/frameworks/kwallet/-/blob/1177d58eb0624dec6a60f79b62188f150a272670/src/runtime/org.kde.KWallet.xml)
reader supports the explicitly selected `org.kde.kwalletd6` or `org.kde.kwalletd5`
service. It enumerates existing wallets, rechecks each name immediately before
`openAsync`, and waits for that transaction's actual-owner `walletAsyncOpened`.
Repeated strings in KWallet `folderList` replies refer to the same API folder
identity: each distinct folder is enumerated once per complete pass and rechecked
as the same sorted identity set. The raw reply remains limited to1024 strings,
including repeats; wallet names, entry keys and Secret Service paths still reject
duplicates. This compatibility normalization does not edit the source.
It copies folder/key/type and the complete `readEntry` bytes for Password=1,
Stream=2 and Map=3. Serialized password/map bytes are preserved without text
conversion. Folder/key/type are retained as encrypted item attributes; full
source key is creator metadata. KWallet supplies no entry timestamps, so these
are stably unknown (zero). Every empty wallet becomes an empty collection;
empty folders have no standalone native object. Only the owned client handle is
closed with force=false. The tool calls no source write/delete/sync method.

KWallet's open API can create a wallet if it disappears between enumeration and
open. There is no atomic open-existing operation in this interface: the operator
must keep providers quiescent. The importer never intentionally requests an
absent wallet, but does not promise byte-identical provider files after provider
unlock/normal maintenance. Original files are retained and never edited directly.

Both readers perform two complete exact passes, with list/property rechecks,
and reject observed mutation. Unlock/handle notifications already incorporated
into the completed passes are discarded; subsequent known data-change signals permanently
retire the captured snapshot during destination passwords/staging. Public legacy
APIs expose no atomic multi-collection export: two equal passes and signal
fencing do not prove a linearizable snapshot against unreported/transient edits.
Keep the source quiescent until publication. No service-name switch is automated.

## Mapping, preservation and bounds

Secret Service collection basenames are retained when safe, at most40 ASCII
filename characters, distinct and not `session`. Other names use a deterministic
source-kind/source-ID hash. Secret Service names win collisions with KWallet;
KWallet `login` is mapped separately from the native fixed login collection.
Original complete IDs remain catalog provenance and item creator metadata.
Item IDs hash the full source item path or the KWallet folder/key pair. Hash or
metadata conflicts refuse the entire batch rather than merge or overwrite.
Aliases follow the native60-character name grammar independently of file IDs.

All KWallet entries remain in their original mapped wallet collection. In
addition, exact64-byte Stream records in `xdg-desktop-portal` are copied into
native fixed login as versioned `legacy-opaque-64` records with exact app ID and
wallet provenance ([ADR-0312](../adr/0312-preserve-exact-legacy-portal-secrets.md)).
The original portal entry remains in the wallet collection. This subset is not
substituted for complete wallet acquisition. Invalid portal records or duplicate
app identities conflict; no truncation, regeneration or KDF is performed. An
unrelated default alias is never moved to make the derived login default.

The existing limits remain:64 persistent collections,1024 items per collection,
1 MiB per secret,32 attributes,4 MiB serialized plaintext per collection,64 known
aliases and128 derived portal records. Strings and wire shapes are bounded.
The private connection caps individual messages at8 MiB, queued source signals
at32 and drained messages at1024; excess retires the reader. Acquisition and
retained reader admission have a ten-minute total budget; individual RPCs use
at most1.5 seconds and owned source prompt waits at most30 seconds. Unsupported
capacity fails explicitly, without a partial success claim or raised storage
limits.

## Native admission, passwords and publication

The CLI requires an explicitly accepted session/display and expected compositor
owner/PID. Public ordinary socket peer/PIDFD attachment and actual-owner native
state receipts must report Unlocked before acquisition and through publication.
Lock, uncertainty, source/session/compositor loss and SIGINT/SIGTERM retire the
operation. There is no KDE locker fallback. Native helper prompting uses an
owned ordinary connection for that exact display, not ambient Wayland discovery.

The default destination password provider is the existing native keyring helper.
It returns bounded owned secure pages and distinguishes cancellation. For
trusted composition/automation, `--password-fd` transfers an anonymous pipe or
same-UID Unix stream. Each destination password is one unsigned32-bit big-endian
length followed by that many bytes; length0 cancels, maximum4096, and one30-second
deadline covers the whole frame. EOF, partial frames and expiry cancel; oversized
frames fail. Ordinary native retirement is checked while waiting. The descriptor
is owned/closed by the importer. Password bytes never appear in argv, logs or
metadata, and are never copied to ordinary text strings.

The destination must have no resident writer. The owning transaction authenticates
all existing collections first and refuses any byte/metadata/provenance/alias
conflict. New sealed files become visible through one catalog publication;
pre-publication failures remove only newly staged unlisted files. Exact retries
are idempotent. A separate same-thread cooperative checkpoint dispatches queued
native retirement/cancellation between staging steps and before publication;
readonly admission remains distinct. A checkpoint cannot destroy dependencies;
nested commit is refused. No event dispatch occurs inside atomic storage rename.
After rename, durability uncertainty reloads locked and reports no success counts.
A final lifetime refusal after publication also claims no success: sealed data
may already exist, so resolve the lifetime/storage failure and retry exactly.

Before any reads the composition disables core dumps and process dumping. Owned
secret/password DTO pages are locked and wiped on disposal. Temporary libdbus
wire allocations and the provider's own memory are not universally locked or
wiped by SecureBuffer; this is an explicit compatibility boundary, not a promise
of framework-wide zeroization. Replies/sessions are disposed promptly. Output
contains only sanitized error codes or aggregate sealed/unchanged counts, never
source IDs, labels or secret contents.

## Reviewed one-time operation

Final Portage packaging supplies the tool and native prompt. The manager/operator
must first retain original stores/backups, select the running providers and exact
session/display lineage independently, keep legacy stores quiescent, and stop
only the native destination writer. Source daemons remain running while copied.
The following shape is reviewable; placeholders are identity/path facts, never
passwords, and do not themselves grant session admission:

```text
qindaqt-keyring-import --bus-address <selected-address> \
  --session-owner <accepted-session-owner> --runtime-root <private-runtime-root> \
  --display <ordinary-display> --compositor-owner <accepted-compositor-owner> \
  --compositor-pid <accepted-compositor-pid> --storage-root <native-private-root> \
  --secret-owner <accepted-secret-service-owner> --secret-pid <accepted-source-pid> \
  --kwallet-owner <accepted-kwallet-owner> --kwallet-pid <accepted-wallet-pid> \
  --kwallet-service org.kde.kwalletd6 --alias <additional-known-alias>
```

Either source may be selected alone; at least one is required. Destination
passwords are requested once per collection in the plan, including authentication
of an exact existing retry. Explicit error codes are InvalidInput1, Capacity2,
Conflict3, Cancelled4, AuthenticationFailed5, OwnerLost6, Unavailable7 and
DurabilityUnknown8; the process returns nonzero on refusal. Successful counts
mean sealed native destination state, not service activation or retirement of
legacy providers. Verify actual app data compatibility before the separate name
switch and overlay retirement gates. Real stores, provider UI behavior, actual
installed helper journeys and either machine's user-data migration are not
qualified by private synthetic fixtures.

See [testing harness](../development/testing-harness.md#native-secret-portal-and-synthetic-legacy-import),
[native daemon](keyring-daemon.md), [storage](keyring-storage.md) and
[Secret portal](secret-portal.md) for adjacent contracts.
