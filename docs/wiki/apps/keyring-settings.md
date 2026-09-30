# Passwords & Keys Settings

The Passwords & Keys route is a native Settings presentation over the public
KeyringGateway and KeyringSettingsModel. It lists Secret Service collections
and authenticated item metadata, while the service owns storage and prompts.
The page never accepts a collection password. Unlock, create, password change,
reveal, and copy invoke native or standard Secret Service prompts through the
client boundary. The revealed secret is shown only while the model's bounded
reveal window remains active; leaving the route deactivates the model and wipes
its retained SecureBuffer.

Copy uses the GUI-only SensitiveClipboard MIME provider. The route acknowledges
a copy only after that provider takes ownership of the secure buffer. Its
secret marker excludes existing clipboard-history capture, its deadline is
bounded, and teardown clears the platform selection only while the provider
still owns it. A clipboard replacement is preserved. Gateway authority loss or
secret invalidation clears the provider.

The page also exposes the typed `keyring.lockOnScreenLock` and
`keyring.lockAfterIdleMinutes` preferences through KeyringPreferences. Controls
remain disabled until Settings1 provides a confirmed baseline and while a
write is pending. A visible availability note reports missing native screen
lock or idle observation. The route does not infer user inactivity from GUI
events, and this presentation alone does not establish resident policy
consumption. Until the policy consumer is integrated and confirmed, the
collections' lock behavior remains owned by Secret Service.

Composition owns a private uniquely named session-bus connection, one scoped
Settings1 client/transport and one KeyringGateway. Model, preference adapter,
clipboard provider, gateway and transport are destroyed before that connection
is disconnected. This route lifetime prevents its request-token domains from
colliding with other Settings routes and prevents it from disconnecting their
bus.

The public client, prompt and clipboard contracts are described in
[Keyring client](../architecture/keyring-client.md),
[Keyring storage](../architecture/keyring-storage.md), and
[ADR-0302](../adr/0302-native-passwords-keys-client-and-policy.md). Settings
route identity and ordering are in [Settings Center](settings-center.md).

The focused presentation checks construct the route through the normal
Settings QML module and verify the empty/degraded state without reading a real
wallet, entering a password, or connecting to a user's Secret Service.
