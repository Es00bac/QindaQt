# Native Secret portal

QindaQt's separate Secret module exports `org.freedesktop.impl.portal.Secret`
version1 from the existing portal resident. Applications use the standard
`org.freedesktop.portal.Secret` frontend; direct backend calls from other bus
actors fail. Appearance remains a separately owned read-only Settings backend.
[ADR-0310](../adr/0310-native-per-application-secret-portal.md) fixes this boundary.

## Standard contract and identity

`RetrieveSecret(o handle,s app_id,h fd,a{sv} options) -> u response,a{sv} results`
uses the upstream [backend XML](https://github.com/flatpak/xdg-desktop-portal/blob/a06ef6127743b59b13e4f3594eb3d90b4796bae9/data/org.freedesktop.impl.portal.Secret.xml).
The backend owns an `org.freedesktop.impl.portal.Request.Close` object at the
provided handle while pending; the frontend owns the application's public
Request and Response signal. Response0 means all bytes were written;1 means
cancellation/deadline;2 means failure. Results are empty. The optional opaque
string token is accepted bounded and does not select another app or item.

Only the current same-UID `org.freedesktop.portal.Desktop` unique owner supplies
`app_id`. QindaQt never derives it from process names, desktop Exec commands or
ordinary application payloads. IDs are case-sensitive ASCII dotted names,
nonempty and at most255 characters. Invalid/empty IDs fail explicitly.

The primary [frontend Secret implementation](https://github.com/flatpak/xdg-desktop-portal/blob/a06ef6127743b59b13e4f3594eb3d90b4796bae9/src/secret.c)
forwards frontend-resolved IDs for both sandboxed and host clients. The standard
[host Registry](https://github.com/flatpak/xdg-desktop-portal/blob/a06ef6127743b59b13e4f3594eb3d90b4796bae9/data/org.freedesktop.host.portal.Registry.xml)
lets an unsandboxed peer register before any portal method. Host IDs retain the
frontend's ordinary host semantics; they do not provide sandbox security
isolation. Unidentified hosts commonly have an empty ID and are unsupported,
rather than sharing an anonymous secret. Compromised same-user host processes
are outside this sandbox identity guarantee.

## Policy, persistence and transport

| Component | Responsibility |
| --- | --- |
| SecretPortalPolicy | Pure bounded identities/options, app-domain record identity and metadata validation, secure random generation |
| Native keyring daemon | Existing login lifecycle, encrypted durable record persistence and native privacy authorization |
| QtKeyringPortalBroker | Borrowed same-thread bus, exact native/SecretService owner admission, authenticated receipts and owned prompts |
| SecretPortalAdaptor | Authenticated frontend, request/rate/deadline policy, standard wire and owned FD writer |
| Resident portal composition | Supplies collaborators before start, retains appearance behavior and existing activation identity |

The owning [keyring daemon](keyring-daemon.md) uses the fixed PK3 collection ID
`login`, regardless of the mutable default alias. If absent with no existing
default, its existing password prompt creates Login through the existing
`default` lifecycle. An unrelated existing default is never moved: absent login
then fails explicitly. A locked login uses its ordinary owning unlock prompt.
No arbitrary collection/item selector or store encryption key is exposed.

An item ID is hexadecimal SHA256 of `QindaQt.SecretPortal1`, a NUL delimiter,
and the exact UTF8 app ID. Full app identity is also retained in authenticated
metadata. Every read checks the ID, exact application/version attributes, MIME
and byte length. A collision or unrelated existing item fails; it is never
silently overwritten. New items contain32 cryptographically random bytes and
version `fresh-native-32`, MIME `application/vnd.qindaqt.portal-secret`, and a
generic label/creator. They persist encrypted through the existing repository
before any bytes are delivered; same-app restart retrieves identical bytes.

Native policy must be admitted Unlocked independently of collection lock
preferences. Unknown/Locked, selected compositor lifetime loss, daemon owner
replacement, frontend owner replacement and bus loss retire pending requests,
dismiss owned prompts and wipe owned SecureBuffers. Resume does not replay an
operation or unlock a collection automatically. Standard Secret Service sharing
remains its separately documented [collection policy](keyring-daemon.md).

`RequestPolicyState(s nonce)` emits targeted `PolicyStateReceipt(s,a{sv})`.
`RequestPortalSecret(s app,s nonce)` emits targeted `PortalSecretResult(s,ay,o)`;
only the exact current portal backend owner can request secret records.
Fresh random UUID nonces, strict signatures, actual signal sender and retained
current native/SecretService owner must all match. Successful RPC replies are
transport acknowledgements and confer no state/secret authority: Qt6.11.1 does
not expose a trustworthy method-reply sender and a real private bus accepts an
unrelated actor's reply for a known incoming serial. The native UI gateway reuses
the same public policy receipt before publishing a reveal. Prompt completion
uses its actual sender and owned path. Final policy readback is a fresh receipt,
not a cached RPC success. Existing native and standard methods remain compatible.

## Bounded publication and memory

At most8 requests are active; handles are unique while pending. The adaptor
tracks at most128 app identities per one-minute window and permits16 attempts
per app per window. Requests/prompts expire within30 seconds. The broker's late
receipt ledger is bounded64 and expires with request timers. Close is accepted
only from the retained current frontend owner and cancels synchronously before
deferred object destruction.

FDs are duplicated close-on-exec. Only writable pipes/FIFOs or stream sockets
are supported; regular files, read-only FDs and datagram sockets fail. Stream
writes use per-call nonblocking/no-SIGPIPE flags. Pipes enable O_NONBLOCK on the
shared open-file description, so the caller observes that flag; SIGPIPE is
blocked/drained/restored only on the writing thread. No global signal behavior
is changed for appearance. Backpressure uses an event-loop notifier and bounded
deadline. Authority is rechecked before each write and successful response.
Already delivered FD bytes cannot be recalled when authority changes later.

Owned temporary wire copies and SecureBuffers are wiped on completion, error,
cancel and late-result retirement. Process cores/dumpability are disabled before
requests; production Qt logging is quiet and diagnostics never include secrets.
D-Bus/framework/kernel/client copies are unavoidable and are not universally
locked or wipeable. This ordinary same-user service does not provide protection
against root or compromised same-user clients of an unlocked collection.

## Legacy compatibility and next boundary

Fresh32-byte native records do not preserve existing portal-encrypted app data.
The current primary [KWallet implementation](https://github.com/KDE/kwallet/blob/1177d58eb0624dec6a60f79b62188f150a272670/src/runtime/ksecretd/kwalletportalsecrets.cpp)
uses networkWallet, folder `xdg-desktop-portal`, exact case-sensitive app entry,
and returns existing opaque bytes verbatim; absent entries generate64 random
bytes. There is no derivation or truncation. KWallet is a separate Secret backend,
not the general KDE portal metadata provider.

PK6 therefore needs a versioned `legacy-opaque-64` import boundary preserving
exact64 bytes, original app identity and provenance, with atomic persistence
and explicit conflict policy. PK5 accepts only fresh-native-32; it cannot claim
migration compatibility or regenerate/truncate legacy records. Synthetic import
qualification precedes manager-controlled installation/live replacement. No
real wallet access, import or installation is part of these tests.

## Private verification

Focused `secret_portal_policy`, `secret_portal_fd_wire`, `secret_portal_broker`
and `secret_portal_native_keyring` rows use synthetic records and isolated real
DBus daemons/FDs. Coverage includes case-sensitive app isolation, durable restart,
first-login/unlock prompts, fixed-login alias independence, malformed metadata,
frontend/native owner replacement, native lock with collection preference false,
cancel/duplicate/rate limits, closed/backpressured/regular sinks, pipe SIGPIPE,
late-prompt dismissal, plaintext buffer wipe and forged RPC/signal receipts.
Installed frontend host registration uses valid staged desktop entries and checks the registration method succeeded before any portal call. Empty-ID behavior is also tested on the
private bus; these are not live desktop or Flatpak sandbox qualification.
See [portal service](portal-service.md) for adjacent appearance gates and routing.
