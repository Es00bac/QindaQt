# Mail Sign in UI candidate handoff

- Worker: `/root/desktop_incident_review` (implementer for this outcome; no self-approval).
- Timestamp: 2026-10-04T18:03:34Z
- Exact candidate: **02c5b4b27a116308ad2c7a6a5223af5651c3a328**.
- Tree: `7e5edbf334d9fbf5a7cfebb7c05ecc0f14fd4a34`.
- Parent/base: installed and hub Office `700d85b7ba73aa6a19d06bdaa3d8512c83c734e6`.
- Branch: `fix/mail-sign-in-dialog-20261004`, pushed to qinda hub; independent `ls-remote` equals exact SHA.
- Isolated worktree: `/home/cabewse/work_space/QindaOffice-mail-sign-in-dialog-20261004`.

## Outcome and boundaries

The real named needsSignIn control emits an account intent. Only its owning MailWorkspace routes it through the public MailHost accountDialogRequested signal into the existing window's AccountDialog. Password and saved Gmail app-password accounts open editable credential fields; Google/missing-client accounts show the actual actionable configuration error. Existing OAuth/save APIs remain the path, with dialog-specific request correlation, visible rejection, and invalidation on dismissal/opening a different account. Opening the dialog itself does not authorize or sync; a second real window sharing the service receives no dialog broadcast or problem.

Changed exactly five owned paths (223 insertions, 8 deletions):

- `apps/qindamail/qml/FolderPane.qml`
- `apps/qindamail/qml/MailWorkspace.qml`
- `apps/qindamail/qml/AccountDialog.qml`
- `tests/qindamail/tst_qindamail_ui.cpp`
- `docs/verification.md`

No service interface, dependency, global event bus, private cross-module QML access, registry, fixture baseline or native adapter path was edited. Shared dirty Office/a89bb2 and unpublished HTML reader/dc6fb7c remain preserved.

## Executable verification

- `cmake --preset dev`: exit 0; QXlsx submodule at exact pinned 4e82d6c0726dcc39020cc5491d0de77f787c18ef.
- `cmake --build build/dev --target tst_qindamail_ui tst_qindamail_host tst_qindamail_accounts -- -j32 -l16`: final exit 0, 15 Ninja steps. Actual laptop `portageq envvar MAKEOPTS` was read as `-j32 -l16` and preserved throughout.
- `QQO_GRAB_DIR=<own ignored build/dev/grabs> dbus-run-session -- ctest --test-dir build/dev -R '^tst_qindamail_(ui|host|accounts)$' --output-on-failure --output-junit <own build/dev/mail-sign-in-final.junit.xml>`: exit 0, **3/3 CTests**, zero failures/skips, 37.96 seconds (accounts 0.36, host 1.48, UI 36.09).
- Actual prior corrected new-case run: Qt **10 passed, 0 failed/skipped**, including setup/cleanup and all eight data cases, 21912 ms. Final host output directly reports Qt **10/10**, zero failures/skips. Longer final successful CTest outputs are truncated at 1024 bytes, so no aggregate Qt total is inferred.
- Existing structural fixture cases pass. All eight final result PNGs inspected at 1280/520: password, Gmail app-password, Google missing-client, configured Google private-flow cancellation. Password fields are usable/masked; actual error text wraps and all footer actions fit. Final two Google/1280 grabs reinspected after settling; no layout blocker.
- `git diff --check`: exit 0. Nonblank line gate: FolderPane126, Workspace49, AccountDialog280, UItest345, all below500. Exact final source equals restored tested candidate; clean Git worktree.
- Commit, push and exact remote SHA checks: exit 0.

### Retained failures and stronger negative control

The initial baseline failed dialog-open without delivered click proof; that interpretation is superseded. Initial repaired CTest exit8 (2/3 suites; UI6pass/8fail) exposed unsettled delegate geometry and focus; these were fixture defects, preserved in logs. A later configured-OAuth assertion wrongly expected browser-opener false to complete the operation; existing OAuth2Session intentionally keeps waiting for manual redirect, so the private fixture now calls its cancellation method after proving the real configured sign-in path.

The stronger unchanged700d negative control restored all three production QML files from exact base only in this isolated worktree, preserving candidate bytes in ignored cache. Build exit0; `dbus-run-session -- build/dev/tests/qindamail/tst_qindamail_ui accountSignInUsesCredentialDialog:password-1280 -v2 ...` returns expected exit1, Qt2pass/1fail/0skip,16886ms. The actual named button's clicked count1 assertion passes before dialog-open fails. Candidate production bytes restored byte-for-byte and final successful rebuild/test follows. Logs: `build/dev/mail-sign-in-delivered-baseline.txt`, `.junit.xml`, `mail-sign-in-final-build.log`, `mail-sign-in-final.junit.xml`.

## Coordinated Mail-page text

`docs/mail.md` ownership belongs to the native credential worker, so this UI candidate does not edit it. Suggested same-integration prose already sent to that worker:

> For a needsSignIn account, FolderPane emits signInRequested(accountId); only its owning MailWorkspace routes that intent through the window's MailHost.accountDialogRequested. The existing account dialog opens that account's password/app-password or browser path, displays its public account lastError (including the actionable missing-client message), and retains rejected sign-in/save results. OAuth operationFinished is correlated to that dialog's returned request ID; dismissal/opening another account invalidates late replies. Opening the dialog itself never authorizes or syncs; shared-service windows receive no dialog broadcast.

## Bounded caveats and requested action

No live app/browser, real credential read/unlock/mutation, user settings, external OAuth/network/send, installation or desktop restart occurs. Password entry/opening and existing browser-config paths are tested; successful external provider authentication and real password server acceptance remain live user/manager gates. The configured-browser fixture intercepts the browser adapter and cancels the private flow; it does not claim a complete external OAuth exchange or redesign the existing manual-redirect fallback. Generic provider/app-password calendar setup is outside this outcome.

Request the different native worker to review **exact02c5b4b27a116308ad2c7a6a5223af5651c3a328** independently before manager integration. Root owns combined docs/registries/integration/Portage. I offer immediate bounded independent source/private-D-Bus review of the native credential candidate at its exact immutable SHA; no unrelated queue work or runtime lease is held.
