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
The Settings page displays a neutral “Locked item” label for those rows and
guards delegate row lifetime during asynchronous metadata replacement.
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

The native route composes the async client, model, typed preferences and a
separate sensitive clipboard owner. The page calls deactivate() on departure;
its synchronous deactivated signal clears the composition-owned clipboard even
while the QML singleton survives navigation. Owner loss and native disclosure
invalidation independently clear the same clipboard owner. See the
[native daemon](keyring-daemon.md), [storage](keyring-storage.md) and
[trusted PAM boundary](keyring-pam.md).

## Typed lock preferences

The separately borrowed KeyringPreferences adapter scopes SettingsClient to keyring.lockOnScreenLock (boolean, default false) and keyring.lockAfterIdleMinutes (integer, 0–1440, default 0 means never). SettingDomain::Keyring is appended without changing existing enum ordinals; the schema stays version 2 and enforces the keyring prefix. Only matching-owner confirmed snapshots change effective values. Pending/uncertain writes never claim persistence, and last confirmed policy remains available as read-only state while Settings1 is unavailable. The route exposes this adapter through model.preferences. The resident daemon consumes confirmed values through the independently admitted native lock and true-idle observers.

## Sensitive clipboard composition

The separate GUI-only KeyringClipboard target exposes SensitiveClipboard. Its MIME provider retains only SecureBuffer ownership and allocates ordinary Qt/platform bytes on actual selection requests. The application/x-qindaqt-secret marker excludes existing clipboard history capture. The provider expires/wipes after 30 seconds; clear/destruction clears the platform selection only while its exact MIME object is still current, preserving replacements. The model reports copied only after composition calls acknowledgeCopy(true). Gateway owner loss and secret invalidation must clear the clipboard owner. Offscreen synthetic tests verify marker, expiry/wipe and replacement preservation. The ordinary native journey below additionally verifies actual transfer and lifecycle clearing. QtWayland may retain format metadata for a wiped source after platform clear: actual empty bytes, rather than marker disappearance, establish clearing. policyStatus reports confirmed native policy availability.

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

## Native Secret portal boundary

The separate [native Secret portal](secret-portal.md) uses fixed login app records and fresh targeted nonce receipts for authenticated policy/secret results. Successful Qt RPC replies confer no native disclosure policy authority. The UI gateway reuses RequestPolicyState/PolicyStateReceipt before publishing revealed bytes; standard Secret Service sharing remains independent. PK5 fresh-native-32 records do not preserve old encrypted app data by themselves. The PK6 synthetic public planner/persistence boundary preserves strict legacy-opaque-64 records and exact version lengths; the separate [one-time importer](keyring-import.md) acquires complete synthetic legacy snapshots through pinned actual-owner replies. Real user-data acquisition and installation/live replacement remain separate delivery gates.

## Native metadata source receipts

The gateway requests bounded `RequestMetadata(s nonce, s kind, o object)`
snapshots and accepts only targeted `MetadataReceipt(s nonce, s kind, v rows)`
with the exact retained daemon sender, expected signature, fresh request nonce,
kind and current owner generation. Collections then obtain a fresh authenticated
policy receipt; that policy does not authenticate unrelated RPC row payloads.
Existing `ListCollections` and `ListItems` remain wire compatible. Forged RPC
replies, forged/stale receipts, duplicates, cancellation and owner loss grant
no native rows.

Reveal never calls `GetSecret` or `GetSecrets`. Its native method reply carries
only an owned prompt path; secret bytes come exclusively from actual retained
owner `Prompt.Completed` at that path, validated against the owned plain session,
and a final fresh policy receipt gates publication. A private bus with the
installed session policy allowances proves that a forged method path reply grants
no foreign Completed bytes; only the actual owner signal produces secretReady.
Standard Secret Service sharing stays separate.

## Ordinary native route journey

`keyring_native_ui_journey` runs the production KeyringPage and
KeyringRouteComposition in a test Window on a real private ordinary Wayland
compositor. Its isolated HOME/XDG roots, bus and display contain only synthetic
records. The private bus has no activation service directories. A test-only executable links the unchanged production prompt main,
controller and QML; Qt test input fills visible fields and clicks approval. The
prompt remains responsible for password protocol output. Every helper verifies
its actual ordinary connection's kernel peer PID and exposed window; no helper
response is scripted and no fixture binary is installed.

The journey creates a collection, unlocks login, reveals/copies a synthetic item,
changes its password, locks/unlocks with the new password, and confirms deletion.
The public sealed store independently proves old-password refusal, new-password
success and persisted deletion. A separate mapped Wayland client receives the
selection. Page departure, actual daemon owner loss and actual native lock each
leave that receiver and the source with empty bytes. Native lock is enabled,
uses the real producer receipt path, and is tested with the confirmed collection
screen-lock preference false. The producer stays alive; the route reports actual
Locked rather than unavailable. Fourteen exposed prompt approvals cover create,
three unlocks, eight reauthentications, rekey and delete.

The qualified private producer source is
`6ab6c01ede8143a7ddb477d6f0040e9b2f3753e4`, production authorization OFF. Native
lock uses its clientless fallback; no trusted launcher admission is claimed.
This journey proves
disclosure retirement, not trusted greeter admission, PAM authentication,
authenticated unlock, physical input, DRM protection, an installed helper, or a
full installed Settings executable. The renderer is a test Window containing the
production route, and input is Qt event injection. The earlier offscreen and
scripted-prompt rows remain distinct evidence. See the
[testing harness](../development/testing-harness.md#ordinary-native-keyring-ui-journey).
