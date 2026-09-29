# Native keyring daemon

`qindaqt-keyring` provides the standard Secret Service and native
`org.qindaqt.Keyring1` interfaces at `/org/freedesktop/secrets`. Its
[storage core](keyring-storage.md) is a separate transport-free target;
the daemon only consumes that public API. This source delivery is qualified
on private buses with synthetic collections. Packaging, live provider
replacement, PAM, full Passwords & Keys Settings, Secret portal routing and
existing-wallet import are separate program boundaries. Nothing in these
tests reads a real wallet or activates a live service.

[ADR-0296](../adr/0296-native-keyring-daemon-boundary.md) supersedes the
provider/lifecycle/prompt choice in ADR-0135 for native deployments. A
distribution must reconcile its Secret Service activation files before
deploying; two providers are not a supported configuration.

## Components and ownership

The repository owns encrypted collection persistence, aliases and catalog
visibility. The virtual-object dispatcher owns wire methods and properties;
session crypto owns only negotiated keys; prompt policy owns owner-scoped
requests; the asynchronous prompt-process adapter owns helper lifetime; and
the peer control server owns one bounded frame per connection. These
collaborators run on one Qt event-loop thread. Borrowed repository item/secret
views expire on mutation, lock, load or destruction and must not cross an
event-loop boundary. Prompt callbacks transfer move-only secure password
buffers. QObject context and ticket generations fence delayed callbacks.

The standard XML in `src/services/keyring/data/api.xml` declares services,
collections, items, sessions, aliases, prompts, properties and signals.
Plain sessions and actual DH negotiation are supported. Encryption follows
the [Secret Service specification](https://specifications.freedesktop.org/secret-service/latest-single/):
RFC2409 group2 DH, a padded 128-byte shared value, HKDF-SHA256 and
AES128-CBC/PKCS7. OpenSSL EVP performs key agreement, HKDF and cipher work.
This legacy wire algorithm has no message authentication and uses a
1024-bit group; it is a compatibility transport, not a stronger replacement
for the authenticated AES256-GCM storage format. Plain sessions deliberately
carry plaintext on the user bus.

All callers must have the daemon's effective UID. Sessions and prompts
additionally belong to a unique bus owner; another connection cannot close a
session, use its key or dismiss its prompt. Disconnect destroys those keys
and cancels active helpers. Unlocked collections are shared by same-user
clients, as required for ordinary Secret Service interoperability.
Fresh locked search results can be unverified. Native `ListCollections`
and `CollectionStateChanged` expose `IndexAuthenticated`; search never
authorizes disclosure and unlock authenticates the complete stored snapshot.

## Persistence and metadata

The daemon holds an exclusive nonblocking flock on a no-follow, singly linked
0600 `.writer.lock` in the euid-owned 0700 collection directory. Even
owners on independent buses cannot concurrently write that directory. Path
components are traversed with no-follow directory descriptors; missing private
components are created 0700 and parent-fsynced. Insecure directories are
rejected, never silently repaired.

A bounded 128KiB version1 `catalog.json` stores only declared collection
IDs, labels, creation/modification dates and alias mappings. These values are
public metadata: a file reader can see them and tamper with them. Item labels,
raw attributes, creators, MIME types and secrets are not copied into the
catalog. The storage index separately leaks counts, equality and dictionary
guesses described on the storage page. Catalog metadata and aliases grant no
decryption authority. Rollback of an older authentic collection remains possible.

The catalog is the cross-file visibility commit. Creation durably saves the
encrypted collection, then publishes its catalog entry and alias. Deletion
durably removes its catalog entry/aliases before unlinking the encrypted file.
A crash between these steps leaves an encrypted orphan, never an advertised
collection. Restart removes undeclared native files under the sole-writer
lock. A referenced missing/corrupt collection fails startup. An absent catalog
may adopt only this module's own native collection files; this is not an import
of another provider's wallet. Catalog publication errors terminate the broker
before another mutation. Save/rekey errors reload and retire uncertain secret
state. A successful reply means the actual durable save was acknowledged;
`DurabilityUnknown` is never reported as success.

Persistent limits inherit PK1; at most64 persistent collections and64 aliases
are admitted. The volatile `session` collection creates no encrypted file,
is bounded to4MiB including metadata, and discards all items on lock.
All KDF admission is serialized with a global minimum500ms gap. Owner-scoped
prompts wait before collecting a password; native rekey waits between old-password
authentication and new derivation.

## Native control and session lifetime

Keyring1 provides `ListCollections() -> a{sv}`, `ChangePassword(o,secret,secret)
-> b`, `AttachSession() -> b` and `Shutdown()`. ChangePassword requires two
secrets using caller-owned sessions, nonempty bounded passwords, and authenticates
the old password. Owner disconnect or session close invalidates delayed work.
It is a bounded seam for later PAM/UI integration, not an implemented PAM module.

The peer socket is `$XDG_RUNTIME_DIR/qindaqt-keyring/control`, mode0600
under a0700 directory with its own writer lock. Kernel `SO_PEERCRED` must
match euid. Each connection sends one header: four bytes `QKR1`, operation
u8, ID-length u8, old/new password lengths as big-endian u16, then ID/old/new
bytes. Operations1/2/3 are unlock/rekey/lock; ID length1–60, each password
at most4096 bytes. No creation or empty-password unlock/rekey is allowed.
Reply is `QKR1` plus byte0 acknowledged or1 rejected. Frames, peer counts
(max8), receive pages and deadlines (2s) are bounded. Passwords stay in secure
receive pages; deferred rekey checks connection generation and disconnect.
Activated descriptor3 is accepted only for one listening Unix socket at the
exact owned mode0600 runtime path.

The session supervisor starts an optional native child only when the native
executable is installed; `qindaqt-session --no-keyring` explicitly disables it
for private/diagnostic runs. A dedicated supervisor bus connection invokes
AttachSession: one unique owner is accepted, including an already activated
daemon; loss of that owner exits and wipes the daemon. Logout stops its child,
sends Shutdown only after successful attachment, and disconnects that bus
connection. Bus loss or loss of Secret Service ownership also terminates the
daemon. Source units use `PartOf=graphical-session.target`; that alone is
insufficient because QindaQt does not activate that target. D-Bus activation
and explicit supervisor ownership are the startup paths. Distribution
`gnome-keyring-daemon` Exec entries are superseded only when this native
executable is selected, preserving the existing polkit filter.

## Prompt and memory boundaries

The default helper is the compiled `qindaqt-keyring-prompt`. It creates a
[shared authentication overlay](authentication-overlay.md) before showing,
uses QindaTK password fields, confirmation for creation, accessible field
names, Enter and Escape, and clears fields after each attempt. Offscreen or
unsupported platforms fail closed; no ordinary password window fallback.
A missing helper cancels, never creates an empty-password collection.
The minimal factory is replaceable by later PK4 scripted/native policy.
Helper arguments contain bounded non-secret request metadata only; approved
password bytes travel over an inherited pipe and are copied into secure pages.

Daemon and helper disable core dumps and set `PR_SET_DUMPABLE=0`.
Owned wire QByteArrays and controller copies are scrubbed on their shortest
practical scope, including rejected decoding. Qt/D-Bus/QML/QProcess, OpenSSL
internals and client applications may allocate independent ordinary copies
that this module cannot lock or fully erase. We do not claim universal secure
memory. No secret is placed in argv, environment, temporary files, logs or
error text. This does not isolate secrets from compromised same-user clients
of an unlocked collection, root, or memory snapshots.

## Verification

Focused CTest rows `keyring_secret_service`, `keyring_prompt_qml`, PK1's four
storage rows, `qindaqt.session-autostart-catalog`,
`qindaqt.session-child-startup` and the polkit dialog/QML gate use disposable
fixtures. Protocol tests execute installed `secret-tool` (libsecret) and
Python SecretStorage, with real encrypted negotiation, persistence/restart,
lock/search/tamper, caller isolation, cancellation/disconnect, control
rekey/bounds/activation, catalog recovery, sole-writer arbitration, session
attachment and dump policy. Native rendering/output selection remains a
nested/live Wayland qualification boundary; no live bus or unit is used here.
