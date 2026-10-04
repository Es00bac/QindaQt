# ACCEPT — exact native Mail credential bridge

- Timestamp: 2026-10-04T18:26:30Z
- Independent reviewer: `/root/desktop_incident_review`; bridge implementer is `/root/native_permission_repair`.
- Verdict: **ACCEPT**; no blocking source finding.
- Exact candidate: **a0cfae24442927ee0abb1a6a47adca2af0d1e111**.
- Tree: `38f8bf4aa01f4dba3948ccdc5dadbadb34c7cad6`.
- Parent/deployed source: `700d85b7ba73aa6a19d06bdaa3d8512c83c734e6`.
- Own detached worktree: `/home/cabewse/work_space/QindaOffice-native-secret-bridge-review-20261004`.
- Exact pushed branch: `fix/native-secret-bridge-20261004`; fetched and separately ls-remote authenticated to the same SHA.

## Findings

Read all twenty changed paths and existing primary SecretBackend/MailSecrets/account removal boundaries. Cohesive client-owned codec, wire types, asynchronous reader and composed backend stay inside office_mail, with private Qt DBus dependency already available to the root build. No private native daemon header, KWallet activation, original mutation/migration, tombstone, file secret backend, insecure QtKeychain fallback, secret logging/QML value exposure or cross-window interface change found.

The primary backend wins; only NotFound creates the import reader. Primary denied/unavailable/failed outcomes remain byte-for-byte outcomes without fallback. No native name initially preserves ordinary missing and successful ordinary remove on other desktops. When present, both aliases must match the pinned unique owner; provider calls disable auto-start, watch owner changes and re-pin before publication. Malformed signatures/types, owner changes, lock/cancel, ambiguous lookup and mismatched authenticated attributes fail closed. Decoder checks the full one-MiB framing and declared length before QString extraction, and rejects null/empty/NUL, malformed UTF-16, truncation/trailing/unsupported sizes. Actual desktop importer source matches folder/key/type1 attributes and application/vnd.kde.kwallet-entry content type. Primary [KDE Entry source](https://raw.githubusercontent.com/KDE/kwallet/master/src/runtime/kwalletbackend/kwalletentry.cc) confirms QString framing; primary [Qt6.11 message source](https://raw.githubusercontent.com/qt/qtbase/6.11/src/dbus/qdbusmessage.cpp) confirms the documented public reply service() limitation. Candidate explicitly uses ordinary same-user client assurance, not raw sender or executable attestation.

Destruction suppresses callbacks, detaches pending readers and drains bounded in-flight resource-creating replies. Known prompt/session cleanup addresses only the originally pinned owner and owned paths. Metadata-only removal probing neither unlocks nor opens a session/read value; a preserved original yields an honest cleanup failure after primary removal. The new app notification forwards that failure visibly after durable account removal; successful cleanup stays silent. D-067 and reciprocal Mail secrets documentation explain these contracts without occupying reserved readerD-066.

## Independent executable evidence

Actual `portageq envvar MAKEOPTS` = `-j32 -l16`; used unchanged for all direct builds. No installations.

1. Standalone `cmake -S tests/office_mail/native_secrets -B build/native-secret-review -G Ninja` exits0; `cmake --build build/native-secret-review -- -j32 -l16` exits0,16steps, compiling actual production backend/reader/codec/wrapper.
2. `ctest --test-dir build/native-secret-review --output-on-failure --verbose --test-output-size-passed 30000 --output-junit .../review.junit.xml` exits0: **2/2 CTests**, observed **72/72 Qt checks** =33 codec +39 backend, zero failures/skips,1.18s. Provider fixture runs only under its own dbus-run-session and temporary HOME/XDG; actual installed QtKeychain leg pins libsecret inside that fixture process.
3. Own pinned QXlsx4e82d6c0726dcc39020cc5491d0de77f787c18ef initialized, `cmake --preset dev` exits0 (existing SQLite imported-target deprecation warnings only).
4. `cmake --build build/dev --target tst_imported_password_codec tst_imported_password_backend tst_mail_accounts tst_mail_autoconfig tst_mail_oauth tst_qindamail_accounts -- -j32 -l16` exits0,**248steps**, compiling actual normal office_mail and static Mail app consumer.
5. `dbus-run-session -- ctest --test-dir build/dev -R '^(tst_imported_password_(codec|backend)|tst_mail_(accounts|autoconfig|oauth)|tst_qindamail_accounts)$' --output-on-failure --verbose --test-output-size-passed 30000 --output-junit .../review-full.junit.xml` exits0: **6/6 CTests**, directly observed **167/167 Qt checks**, zero failures/skips,**3.53s**. Counts: module accounts39,autoconfig20,OAuth18,codec33,private backend39,app accounts18.
6. Actual QtKeychain plaintext/base64 miss recovery and native precedence, missing-native-name compatibility, invalid codec/attributes/duplicates, locked/prompt/refusal/cancel/still-locked paths, owner mismatch/replacement, in-flight session/prompt cancellation, before-start destruction, primary-only writes, preserved-import cleanup failure and successful silence pass. Existing Mail account/OAuth/autoconfig cases pass in private fixtures; loopback test services replace external providers.
7. Exact identity/tree/parent, remote SHA, clean worktree, `git diff --check HEAD^ HEAD`, changed source/test nonblank<=500 and two new local docs link/anchor checks exit0. Own cwd-scoped compiler/test/private-bus process scan exits0 with **zero survivors**.

Logs are own ignored `build/native-secret-review/review-dev-configure.log`, `review-full-build.log`, `review-test.log`, `review-full-test.log`, `review.junit.xml` and `review-full.junit.xml`. Initial shell wrappers used zsh's readonly status variable and exited1 after commands; corrected wrappers/reruns provide the direct exit0 evidence above. No source changes were needed. Author's disabled-bridge expected-failure control is retained in his exact handoff, not claimed as my rerun.

## Changed paths reviewed

- `apps/qindamail/src/mail_service.cpp`
- `docs/decisions.md`
- `docs/mail.md`
- `libs/office_mail/CMakeLists.txt`
- `libs/office_mail/src/imported_password_backend.cpp`
- `libs/office_mail/src/imported_password_backend.h`
- `libs/office_mail/src/imported_password_codec.cpp`
- `libs/office_mail/src/imported_password_codec.h`
- `libs/office_mail/src/imported_password_reader.cpp`
- `libs/office_mail/src/imported_password_reader.h`
- `libs/office_mail/src/imported_password_wire.h`
- `libs/office_mail/src/secret_backend.cpp`
- `libs/office_mail/src/secret_backend.h`
- `tests/office_mail/CMakeLists.txt`
- `tests/office_mail/native_secrets/CMakeLists.txt`
- `tests/office_mail/native_secrets/synthetic_secret_service.cpp`
- `tests/office_mail/native_secrets/synthetic_secret_service.h`
- `tests/office_mail/native_secrets/tst_imported_password_backend.cpp`
- `tests/office_mail/native_secrets/tst_imported_password_codec.cpp`
- `tests/qindamail/tst_qindamail_accounts.cpp`

## Caveats and next action

All runtime evidence is synthetic/private. It does not prove real imported credential bytes, native live unlock, successful provider authentication or external Mail acceptance. No live credential/account/settings read/change, browser/auth/network/send, application/physical/PAM execution, package installation or desktop restart was performed. Plain Secret Service transport and ordinary QString callback memory match the explicitly documented same-user boundary; no secure-memory/raw transport attestation is claimed. Known Close/Dismiss cleanup is best effort; unknown resource handles after transport timeout are not asserted closed. Overall70-second/prompt timeout is source-inspected rather than a live70-second test.

Root may integrate exacta0cf with the independently accepted UI02c5, add coordinated Mail-page UI prose and rerun combined affected gates before Portage adoption. Source acceptance does not itself claim integrated/package/live completion. I offer bounded exact combined Office source/recipe/artifact/provenance review for this incident when requested; no unrelated queue work, live process or runtime lease remains.
