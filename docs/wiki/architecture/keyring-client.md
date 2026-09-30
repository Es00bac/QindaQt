# Native keyring client and Passwords & Keys model

[ADR-0302](../adr/0302-native-passwords-keys-client-and-policy.md) defines the
native Settings client boundary. Public protocol types live in
src/services/keyring_protocol; KeyringGateway and QtKeyringGateway live in
src/services/keyring_client; the transport-free KeyringSettingsModel lives in
src/apps/settings/keyring.

All objects are confined to one GUI/event-loop thread. The connection outlives
the gateway; the gateway outlives its model. Operations are asynchronous and
echo a nonzero caller token. Cancellation, bus loss and owner replacement fence
late replies, destroy the owned plain session and dismiss owned prompts.
There is no service activation or direct storage access in the client.

Collections/items are validated bounded metadata maps. Item label, captured
creator and dates are available only while unlocked and authenticated. Fresh
locked search rows remain explicitly unverified and cannot authorize a reveal.
Collection labels and aliases are public metadata. Creator is a captured
basename hint, never a trustworthy executable identity.

Create/unlock/rekey/reveal/delete use native or standard Secret Service
prompts. The page never accepts a password. Reveal and copy require fresh
native reauthentication even for unlocked collections. A durable mutation
must have an explicit successful prompt acknowledgement; failure, cancellation
and uncertain saves remain visible errors. No mutation is automatically replayed.

The model owns a revealed SecureBuffer until clear, timeout (15 seconds),
page departure, next action or owner loss. Text exposed to QML is an unavoidable
ordinary Qt/UI copy. Neither D-Bus nor QML promises to wipe every independent
allocation. The process disables cores/dumpability before secret requests;
owned wire byte copies are scrubbed. Plain sessions carry plaintext on the
same-user bus and provide no cryptographic transport authentication.

The current checkpoint delivers native prompt operations and client/model
interfaces; Settings presentation and resident policy are the next slice.
The composition must call deactivate() on departure and provide a secret MIME
clipboard owner with a bounded clear deadline. See the
[native daemon](keyring-daemon.md), [storage](keyring-storage.md) and
[trusted PAM boundary](keyring-pam.md).

## Typed lock preferences

The separately borrowed KeyringPreferences adapter scopes SettingsClient to keyring.lockOnScreenLock (boolean, default false) and keyring.lockAfterIdleMinutes (integer, 0–1440, default 0 means never). SettingDomain::Keyring is appended without changing existing enum ordinals; the schema stays version 2 and enforces the keyring prefix. Only matching-owner confirmed snapshots change effective values. Pending/uncertain writes never claim persistence, and last confirmed policy remains available as read-only state while Settings1 is unavailable. The route exposes this adapter through model.preferences. Resident policy consumption remains the next slice.

## Sensitive clipboard composition

The separate GUI-only KeyringClipboard target exposes SensitiveClipboard. Its MIME provider retains only SecureBuffer ownership and allocates ordinary Qt/platform bytes on actual selection requests. The application/x-qindaqt-secret marker excludes existing clipboard history capture. The provider expires/wipes after 30 seconds; clear/destruction clears the platform selection only while its exact MIME object is still current, preserving replacements. The model reports copied only after composition calls acknowledgeCopy(true). Gateway owner loss and secret invalidation must clear the clipboard owner. Offscreen synthetic tests verify marker, expiry/wipe and replacement preservation; native compositor transfer qualification is a later manager gate. policyStatus is an additive availability label for enabled resident observations; until wired/confirmed it says unavailable.

## Native disclosure privacy

The gateway pins one same-UID owner for both Secret Service and Keyring1.
Native policy loss, Locked or Unknown retires a pending reveal even without a
presentation model. The client rechecks current pinned-daemon policy before
publishing prompt bytes. Native reveal publication requires the daemon's
independently admitted exact compositor owner/PID to remain Unlocked; the
collection screen-lock preference does not relax this disclosure gate. The
model exposes `secretsAllowed`, rejects stale/unadmitted replies and clears
revealed bytes on invalidation. Composition must clear its SensitiveClipboard
on both authority loss and secret invalidation. Standard Secret Service
sharing remains the separate collection-lock policy contract.
