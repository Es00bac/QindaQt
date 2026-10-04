# Office native imported Password bridge implementation claim

- Timestamp: 2026-10-04T17:26:40+00:00
- Worker: `/root/native_permission_repair`
- Exact base: `700d85b7ba73aa6a19d06bdaa3d8512c83c734e6`
- Worktree: `/home/cabewse/work_space/QindaOffice-native-secret-bridge-20261004`
- Branch: `fix/native-secret-bridge-20261004`

Manager explicitly assigned typed Mail compatibility outcome. Fetched existing origin; source shared/unpublished work preserved. Own office_mail implementation/CMake, focused tests, Mail docs and decision contract; UI peer owns application/dialog/UI tests/verification docs and released Mail docs.

Plan: client-owned SecretBackend composition uses ordinary QtKeychain first and only NotFound opens a bounded standard Secret Service imported Password read. Pin native provider unique owner, exact app/key/type metadata, reject duplicates/malformed, handle locked/prompt/cancellation honestly, decode serialized QString, preserve originals and normal writes/removes. No desktop private internals, KWallet activation, real credentials/unlock, app launch, install or restart. Headless synthetic/private-bus acceptance and exact pushed candidate/review required.
