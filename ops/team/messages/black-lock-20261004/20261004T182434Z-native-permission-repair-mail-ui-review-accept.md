# Independent exact Mail Sign in UI review — ACCEPT

- Timestamp: 2026-10-04T18:24:34+00:00
- Reviewer: `/root/native_permission_repair`, distinct from UI implementer `/root/desktop_incident_review`.
- Verdict: **ACCEPT exact02c5b4b27a116308ad2c7a6a5223af5651c3a328**, no blocking source findings.
- Tree: `7e5edbf334d9fbf5a7cfebb7c05ecc0f14fd4a34`; parent/deployed base `700d85b7ba73aa6a19d06bdaa3d8512c83c734e6`.
- Own detached review worktree: `/home/cabewse/work_space/QindaOffice-mail-ui-independent-review-20261004`; exact product source clean throughout/after review.

## Evidence and source findings

Read all five exact changed files and the public MailHost/MailService/Qt toolkit dialog contracts. The actual delivered named click emits only window-scoped intent, preserving shared service separation. Saved Gmail app-password mode reopens correctly; browser mode restores xoauth2 explicitly. Public account-specific lastError is visible, save/sign-in rejections stay in the existing dialog, pending primary action is disabled, and only that dialog's returned operation ID can finish/reopen it. Escape/Cancel clears IDs; opening another account resets IDs and state. No new dependency or private service interface is introduced.

Own configure/build of actual source: `cmake -S . -B .cache/ui-review-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQQO_STRICT_WARNINGS=ON -DBUILD_TESTING=ON`, `cmake --build .cache/ui-review-build --target tst_qindamail_ui tst_qindamail_host tst_qindamail_accounts -- -j32 -l16`: exit0, configured MAKEOPTS unchanged. Exact QXlsx initialized in own tree only.

Own `QQO_GRAB_DIR=<own ignored review-build/grabs> dbus-run-session -- ctest --test-dir .cache/ui-review-build -R '^tst_qindamail_(ui|host|accounts)$' --output-on-failure -V`: exit0, **3/3 CTests,40/40 Qt checks**, zero failures/skips (accounts16, host10, UI14). All eight delivered-click password/app-password/missing-client/configured-OAuth cases pass at1280/520. Existing structural fixtures pass.

Inspected all eight own final PNGs directly: both password modes show usable masked focused fields; actionable missing-client and real synthetic cancellation notices wrap; footer actions remain visible at520 and1280. No clipping/layout blocker.

A supplemental review-only test under ignored `.cache/ui-dismissal-probe` compiles the exact product library plus a copied fixture with extra assertions, registered through an ignored CMake project hook. It preserves every tracked candidate file. For configured OAuth at both1280/520: unrelated operation ID does not clear pending state/reopen another window; real Escape emits rejected and clears signInId; actual OAuth cancel completion arriving later cannot reopen either window. `dbus-run-session -- .cache/ui-review-build/tst_mail_ui_dismissal accountSignInUsesCredentialDialog:google-configured-1280 accountSignInUsesCredentialDialog:google-configured-520`: exit0, **4pass/0fail/0skip** including setup/cleanup. This closes dismissal/correlation coverage beyond the implementer's eight cases.

`git diff --check`, exact HEAD/tree/parent and clean source gates pass. All five changed files below500 nonblank lines. Retained author unchanged700d delivered-click negative control is consistent with exact diff: clicked1 then closed-dialog failure; author handoff preserves that expected exit1 and earlier fixture failures, not mislabelled product successes.

## Limits and requested action

Synthetic private XDG/keyring/bus only; intercepted browser adapter and local cancellation, no external OAuth, real credentials, user settings, installed app or physical desktop actions. Dismissal invalidates late UI replies; it does not claim to cancel already-started external browser authentication or change backend authentication lifecycle. Successful hosted authentication/password-server acceptance remains user/manager qualification. The native bridge is a separate exact candidate with a different reviewer; this verdict does not self-approve it.

Root may integrate exact02c5b4b and the separately accepted native bridge, add coordinated Mail-page prose, and rerun combined affected gates before Portage adoption. Review/compiler/private-bus leases are released, observed owned argv scope zero survivors.
