# Office native imported Password bridge — exact candidate handoff

- Timestamp: 2026-10-04T18:24:34+00:00
- Implementer: genuine collaboration agent `/root/native_permission_repair`; no self-approval.
- Exact candidate: **a0cfae24442927ee0abb1a6a47adca2af0d1e111**.
- Tree: `38f8bf4aa01f4dba3948ccdc5dadbadb34c7cad6`.
- Parent/exact deployed base: `700d85b7ba73aa6a19d06bdaa3d8512c83c734e6`.
- Isolated worktree: `/home/cabewse/work_space/QindaOffice-native-secret-bridge-20261004`.
- Branch: `fix/native-secret-bridge-20261004`, pushed to qinda hub; independent ls-remote equals exact SHA; source worktree clean.

## Outcome and contract

Normal native QtKeychain reads stay first. Only NotFound can recover an exact generic imported qindaoffice-mail folder / validated accountId-service key / KWallet Password type1 record through standard Secret Service. Actual QtKeychain/libsecret user-server-type plaintext/base64 schema miss and native precedence are exercised on a private bus. Imported bytes are bounded typed QDataStream QString, not re-labelled plaintext. Null/empty/NUL, malformed UTF-16, declared-size attacks, incomplete/trailing frames and unsupported record/content types fail closed. Originals are unchanged.

Absent native name initially leaves ordinary missing read and successful ordinary remove unchanged on other desktops. Otherwise standard/native names must share the pinned unique owner; all provider calls and cleanup address that original unique name, with activation disabled, owner-change observation and final re-pin. Qt6.11 [service() source](https://raw.githubusercontent.com/qt/qtbase/6.11/src/dbus/qdbusmessage.cpp) intentionally hides method reply sender: this is ordinary same-user client assurance, not raw sender/executable attestation. No new libdbus or private keyring dependency.

Locked imports use the provider's standard Unlock/Prompt. Decrypted attributes and uniqueness are verified before typed read. Primary unrelated errors remain unchanged. Cancellation/refusal/still-locked conditions report Denied; malformed/ambiguous/protocol failures reject access. Callback ownership, creating event-loop thread, retained primary lifetime, 2-second calls/70-second overall bound, and cancellation during queued start/in-flight prompt/session creation are explicit and tested. Known owned resources are closed/dismissed on the original pinned owner; Close/Dismiss are best effort standard calls, not a remote persistence-deletion promise.

Writes remain primary. Ordinary remove cannot delete the preserved imports: a metadata presence probe without unlock/value/session returns explicit incomplete-cleanup failure when one remains. Root separately assigned MailService's minimal existing notify connection so AccountStore's asynchronous failure is visible after durable removal; successful cleanup has no warning. No tombstones, original mutation/migration, logs/QML secret values or disk plaintext fallback.

## Changed paths

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

All hand-written changed product/test files are below500 nonblank lines (reader267, backend70, codec45, backend test387, service444, account test338). D-067 avoids the preserved unpublished HTML reader's D-066. Shared dirty Office checkout, clean dc6fb7c reader and other unpublished work were not edited.

## Exact executable verification

- Actual `portageq envvar MAKEOPTS`: `-j32 -l16`; equivalent configured limits used unchanged. No package installation.
- Normal configure: `cmake -S . -B .cache/office-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQQO_STRICT_WARNINGS=ON -DBUILD_TESTING=ON`: exit0. Exact vendored QXlsx `4e82d6c0726dcc39020cc5491d0de77f787c18ef` initialized only in owned worktree.
- Final normal build: `cmake --build .cache/office-build --target office_mail tst_imported_password_codec tst_imported_password_backend tst_mail_accounts tst_mail_autoconfig tst_mail_oauth tst_qindamail_accounts -- -j32 -l16`: exit0. Compiles actual library, static Mail app consumer and all six gates.
- Final normal gate: `ctest --test-dir .cache/office-build -R '^(tst_imported_password_(codec|backend)|tst_mail_(accounts|autoconfig|oauth)|tst_qindamail_accounts)$' --output-on-failure -V`: exit0, **6/6 CTests**, **167/167 Qt checks**, zero failures/skips. Individual counts: accounts39, autoconfig20, OAuth18, codec33, private-bus backend39, app accounts18. Logs `.cache/office-build/mail-final-build.log` / `mail-final-gates.log`.
- Final standalone: `cmake -S tests/office_mail/native_secrets -B .cache/native-secrets-build -G Ninja -DCMAKE_BUILD_TYPE=Debug`, equivalent configured build, `ctest --test-dir .cache/native-secrets-build --output-on-failure -V`: exit0, **2/2 CTests,72/72 Qt checks**, zero failures/skips. Tests require QtKeychain with libsecret; only the private fixture process pins QTKEYCHAIN_BACKEND=libsecret and temporary HOME/XDG.
- Negative control: ignored copy of exact candidate production/test bytes, with only withImportedPasswords factory changed to return its primary (bridge disabled). Configure/build exit0. `env QQO_MAIL_PRIVATE_SECRET_TEST=1 dbus-run-session -- .cache/native-secret-negative/build/tst_imported_password_backend actualQtKeychainSchemaMissRecoversImport`: expected exit1, **2pass/1fail/0skip**; actual primary status NotFound, expected recovery Ok. Exact final reader/test bytes refreshed before final control. Logs `.cache/native-secret-negative/expected-failure.log` / disabled-bridge.diff.
- Local Mail/decisions relative links and reciprocal D-067 anchors: exit0; `git diff --check`: exit0; commit/push/remote exact SHA and clean source checks: exit0.
- Owned cache compiler/test/private-bus argv scope scan: zero matching survivors after gates. No physical desktop/GPU/VT/PAM/live app or keyring operations.

## Bounded caveats and requested next action

No live credential read/unlock/mutation, account configuration changes, GUI launch, external authentication/network/mail send, installation or restart were performed. These gates use synthetic fixtures only; they do not prove real imported credential content or provider authentication. Plain Secret Service session and ordinary QString callback memory match the ordinary client boundary, without secure-memory/transport attestation claims. Unknown handles after a transport timeout cannot be claimed as remotely closed; known resource cleanup is best effort and client disconnect remains the standard provider lifecycle backstop. Full Office suite/package adoption and combined UI/native integrated gates are manager-owned.

Different worker `/root/desktop_incident_review` is reviewing this immutable SHA. Request independent verdict before root integration and Portage adoption. Source is pushed; no further candidate changes without exact SHA notification/re-review.

Peer UI text for manager's same integration (not included prematurely in this source-only candidate): for needsSignIn, FolderPane emits signInRequested(accountId) and the owning MailWorkspace routes through its window MailHost.accountDialogRequested. Existing account dialog opens that account's password/app-password or browser path, displays account lastError and retains rejected results. OAuth reply uses that dialog request ID; dismissal/another account invalidates late UI replies. Opening the dialog does not authorize/sync; other shared-service windows receive no dialog broadcast. See independent accepted exact02c5b4b verdict in adjacent reply.
