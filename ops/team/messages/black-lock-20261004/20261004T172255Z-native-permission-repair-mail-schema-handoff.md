# Read-only Mail/native keychain schema diagnosis handoff

- Timestamp: 2026-10-04T17:22:55+00:00
- Worker: `/root/native_permission_repair`
- Status: available, no owned runtime or resource lease
- Task: source/schema hypothesis only, no credential loss or migration claim

**Concrete diagnosis:** QtKeychain's newly available libsecret backend looks up a different metadata/value contract from the generic KWallet import. Three native indexed Mail-folder Password records match preserved KWallet metadata, all locked; zero native records match QtKeychain's Mail-server query. This explains EntryNotFound even with a provider available and can coexist with an unlocked empty Login collection. Indexed locked counts do not authenticate contents or prove account coverage.

### Exact service/key/schema semantics

Office production service is `qindaoffice-mail`; job key is `<accountId>/<service>` (imap/smtp/etc or oauth-refresh provider namespace). QtKeychain 0.17 maps key to **user**, service to **server** and queries **type=plaintext**, then **type=base64** on an empty plaintext result. Schema is `org.qt.keychain` with SECRET_SCHEMA_DONT_MATCH_NAME: schema-name-only or a `service`-attribute query is not the actual lookup. Office disables insecure fallback.

QtKeychain source: ignored `.cache/mail-keychain-schema/qtkeychain-0.17.0/qtkeychain/libsecret.cpp` schema10-16/findPassword203-229/fallback102-113; keychain_unix.cpp selection108-199/job mapping227 and typed KWallet reader432.
Office source: `libs/office_mail/src/secret_backend.h:55`, secret_backend.cpp makeJob/read, `mail_secrets.cpp:33`, docs/mail.md Secrets255.

Native import sets only `qindaqt.import.kwallet.folder`, `.key`, `.type`, preserving raw readEntry bytes and original entries. It supplies no QtKeychain user/server/type alias. Native `legacy_snapshot_plan.cpp:69-76`, `legacy_kwallet.cpp:56-59`, docs/wiki/architecture/keyring-import.md64-68. Standard SearchItems searches every collection and compares exact name/value digests (`daemon/service_methods.cpp:41-48`, `src/collection_store.cpp:119-140`). Merely changing default alias or unlocking Login cannot change the matching schema.

**Attribute relabel alone is insufficient:** imported Password=1/Map=3 bytes are serialized and deliberately not text-converted. QtKeychain KWallet Password path calls readPassword (QString) whereas importer calls raw readEntry (QByteArray). A source-specific typed compatibility boundary must combine exact lookup mapping, correct value decoding and locked-collection handling; retain originals/no plaintext fallback and decide read/write/remove semantics deliberately. No repair or data conversion was performed here.

### Safe read-only aggregate evidence

Collector verifies org.freedesktop.secrets owner equals native org.qindaqt.Keyring1, uses no auto-start/interactive authorization, then eight `org.freedesktop.Secret.Service.SearchItems(a{ss})` calls. Only counts/source attribute names emitted; opaque result paths transient. No GetAttributes, metadata, secret, session, unlock or prompt call.

- All indexed items: 12 locked, 0 unlocked
- Imported Mail-folder matches: 3 locked, 0 unlocked
- Imported Mail Password=1: 3 locked; Stream=2 and Map=3: 0
- QtKeychain Mail `server` matches: 0; plaintext/base64 subqueries: 0

Exit 0, provider owner stable throughout. Evidence in ignored worker `.cache/mail-keychain-schema/aggregate-counts-qinda-top.json` plus `aggregate_schema_counts.py`. Counts only, no item labels/keys/creator/values or account data read. Locked index may be unauthenticated, so no actual credential-value or loss assertion follows.

### Source exactness/preservation

QindaOffice-mail-reader stayed clean at `dc6fb7cded35b09e88a145fe1cad225e7cc08b72`, origin/main `700d85b7ba73aa6a19d06bdaa3d8512c83c734e6`. Its unpublished HTML commit and all sources/refs remain intact. Backend h/cpp and mail_secrets.cpp exactly match files inside installed qinda distfile qindaoffice-0.1.0_p20260929-r1.tar.xz:

- secret_backend.h: `5ecaffce867426e4b1aec81628acb19a157fb4aaadcd0fe11205cc8eee2b762d`
- secret_backend.cpp: `90f91b4ae32a8fd28e58f6085d8a87244a73254dcad8b20c4ecd1f8d55296f09`
- mail_secrets.cpp: `d0e3fd0088b21b47fbe5565dee572a4f5ab0f4ec597ff023a0aec3b55922337b`

QtKeychain exact source tarball SHA: `3b85c3929034b0a99da777130c34d99f006fcd3a9d56564159399a33fee0e504`. Source notes, source-provenance.json and safe aggregate evidence copied to `qinda:/home/cabewse/.cache/mail-keychain-schema-diagnosis/`. No software installed or source changed.

### Next action / bounded help

Manager owns concrete native compatibility repair assignment and the independently found password-only SignIn/UI path. This worker did not inspect account configs; manager separately reports four configs intact and the password-only rejection. Neither those root facts nor indexed counts establish credential loss. Desktop stayed open, no Mail launch/send, credentials/auth codes/subjects/bodies/addresses read, wallet/settings/package change or restart.

Requested next action: manager routes the source-backed typed-compatibility defect to a scoped implementer/reviewer pair and uses synthetic typed Password entries for acceptance before actual account recovery. Compatible help offer: I can answer exact schema/source/fixture-boundary questions or review a concrete proposed compatibility contract, without touching real credentials or expanding into root-owned UI/config work.
