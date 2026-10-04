# Mail keychain schema mismatch: source and safe counts

- Timestamp: 2026-10-04T17:16:19+00:00
- Worker: `/root/native_permission_repair`
- Outcome: read-only evidence, no fix/migration claim

QtKeychain 0.17 libsecret schema is `org.qt.keychain`, DONT_MATCH_NAME; lookup attrs are `user` (job key), `server` (job service), `type` first plaintext then base64. QindaOffice backend fixes service `qindaoffice-mail`, key `<accountId>/<service>` and disables insecure fallback. Installed p20260929-r1 tarball independently confirms these constants.

Native KWallet importer preserves generic `qindaqt.import.kwallet.folder`, `.key`, `.type` attributes and complete raw readEntry bytes. It does not expose QtKeychain user/server/type alias attrs. Import docs explicitly preserve serialized password/map bytes without text conversion. QtKeychain's KWallet Password reader uses readPassword (QString), unlike raw readEntry. Attribute relabel alone is therefore not a correct typed adapter.

Safe aggregate script pins org.freedesktop.secrets owner to org.qindaqt.Keyring1 and issues only eight standard SearchItems calls, no activation, metadata, secret, OpenSession, unlock or prompt. On qinda-top: 3 generic imported Mail-folder Password=1 matches, all locked; 0 matches for QtKeychain server or plaintext/base64 queries; total 12 indexed items, all locked. Only counts and source attribute-key names emitted. Stored JSON and helper in ignored worker `.cache/mail-keychain-schema/`. Locked search metadata can be unverified, so these counts prove a lookup-schema gap, not authenticated credential contents or loss.

Manager separately confirms four saved account configs intact/mtime unchanged and password-only SignIn rejection in Mail UI. Those are root-owned source/config facts; this worker did not read account configs or credentials and makes no account-content or credential-loss claim.
