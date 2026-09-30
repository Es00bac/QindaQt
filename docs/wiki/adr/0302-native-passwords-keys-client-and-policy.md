# ADR-0302: Native Passwords & Keys client and prompt policy

- Status: Accepted
- Date: 2026-09-29
- Owners: QindaQt program manager and keyring module
- Scope: Public client, native prompt operations and later Settings presentation

## Context

Standard Secret Service shares unlocked secrets between same-user clients.
An explicit Settings reveal/copy action needs fresh authentication even when
a collection is already unlocked. Settings must not capture collection
passwords or reach into daemon storage. Ordinary Qt/D-Bus and QML allocations
cannot honestly be described as universally locked or scrubbed memory.

## Decision

Separate public wire values (src/services/keyring_protocol), an asynchronous
owner-fenced client (src/services/keyring_client), and a pure presentation
model (src/apps/settings/keyring). The gateway borrows a same-thread bus
connection; the model borrows a gateway that outlives it. A nonzero operation
token and service-generation fences prevent late replies after cancellation,
page departure, owner replacement or bus loss. No mutation is replayed.

Native Keyring1 adds ListItems, ReadSecretWithPrompt,
ChangePasswordWithPrompt and DeleteItemWithPrompt. Prompts belong to one
unique caller. ReadSecretWithPrompt authenticates the real collection password
and returns one Secret Service wire secret targeted to that caller's owned
session. An already-unlocked state and a locked search index never substitute
for this authentication. The Settings client uses a standard plain session;
the user bus therefore carries plaintext, just as ordinary plain clients do.
Same-UID bus ownership is ordinary user-service authority, not the stronger
PAM activation proof in [ADR-0300](0300-trusted-native-keyring-pam.md).

The native helper collects old/new password pairs for rekey and explicit
deletion approval. Password frames travel over anonymous stdout pipes;
bounded label/caller metadata travels over stdin, never argv/environment.
QKP1 frames have exact bounded old/new lengths; QMP1 metadata has bounded,
validated UTF-8 fields and an absolute read deadline. Encrypted item labels
stay out of process listings. Deletion/rekey success requires actual durable
save acknowledgement; cancellation, owner loss and uncertain persistence
cannot become success. Rekey cancellation restores an initially locked state.

Item creator is captured at creation through the authenticated caller PID
when readable, otherwise its unique bus name. It is an informational basename
hint, not executable identity attestation. Existing encrypted ItemMetadata
already owns this field; no public attribute or storage format is invented.
Unlocked ListItems returns authenticated label/creator/dates. Locked rows
contain only item path, Locked and IndexAuthenticated and never authorize
disclosure. Collection labels/aliases remain declared public metadata.

The presentation model receives secure byte ownership only after native
reauthentication. Reveal is bounded to 15 seconds and cleared on departure,
service loss and the next action. Password input never enters Settings.
Explicit Qt/UI display copies may outlive an owned buffer; core suppression,
short lifetimes and owned-copy wiping are mitigation, not a universal secure
pages claim. Copy policy is composition-owned and must mark its MIME as secret,
exclude clipboard history, and clear only its own still-current selection.

## Consequences and next boundary

The native protocol/prompt/client/model form a reviewable vertical checkpoint.
The current candidate also supplies typed lock preferences and resident
native screen-lock/true-idle consumption, qualified on private fixtures. Public
ordinary attachment/idle modules are now consumed through their public boundaries
([ADR-0305](0305-public-ordinary-compositor-attachment.md),
[ADR-0307](0307-public-ordinary-fd-idle-observation.md)); keyring retains selected-session
and legacy fallback policy only. Settings route integration and rendered Wayland
disclosure qualification remain the next PK4 acceptance boundary. Defaults are
keyring.lockOnScreenLock=false and keyring.lockAfterIdleMinutes=0 (never);
logout continues to retire all secrets. Settings1 stores only typed policy,
never passwords or item values. A preference acknowledgement must reflect a
confirmed Settings1 commit; uncertain writes are not automatically retried.

Private real-daemon and synthetic model tests exercise prompt ownership,
wrong-password failure, actual durability, metadata trust, cancellation and
short-lived reveal ownership. No live keyring, system PAM stack or installed
unit is involved. Native rendered overlay/output qualification remains bounded
by [ADR-0300](0300-trusted-native-keyring-pam.md).

The GUI clipboard composition is now a separate public KeyringClipboard target. Its secure MIME provider marks selection data as secret, expires after 30 seconds and wipes owned bytes, preserving an unrelated replacement selection. The model requires actual sink acknowledgement before saying copied. Independent Qt/platform/requester copies remain explicitly outside universal wiping guarantees.

Resident policy retains only last confirmed typed Settings1 state across owner
loss. Enabled screen or idle policy locks collections on uncertainty, and true
idle comes from an ordinary compositor connection, never a local elapsed timer.
Resume never unlocks. Additive GetPolicyState/PolicyStateChanged report bounded
availability and effective policy. Native Settings disclosure separately requires
independently admitted native Unlocked, regardless of collection-lock preference;
privacy loss retires prompts and pending gateway results and clears owned bytes.
The final prompt encoding and client publication each recheck live admission.
Standard Secret Service sharing remains governed by collection-lock policy.

## Source provenance follow-up

A private installed-policy bus reproduced foreign native metadata method replies
accepted before an authentic policy receipt. The native gateway now consumes
actual-owner nonce/kind-correlated MetadataReceipt snapshots; legacy native list
RPCs remain compatible. Existing actual-owner owned Prompt.Completed continues
to supply Reveal bytes, followed by a fresh policy receipt. A forged method
path acknowledgement cannot grant foreign prompt bytes. This bounded native UI
repair changes no standard Secret Service protocol or shared transport framework.
