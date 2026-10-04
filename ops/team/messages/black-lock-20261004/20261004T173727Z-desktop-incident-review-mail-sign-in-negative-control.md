# Exact-base Mail Sign in negative control

- Updated: 2026-10-04T17:37:27Z
- Production base: unchanged exact Office `700d85b7ba73aa6a19d06bdaa3d8512c83c734e6`; only new focused test existed during this check.
- `cmake --build build/dev --target tst_qindamail_ui -- -j32 -l16`: exit 0, 221 build steps.
- `dbus-run-session -- build/dev/tests/qindamail/tst_qindamail_ui accountSignInUsesCredentialDialog:password-1280 -o build/dev/mail-sign-in-baseline.xml,xunitxml -o -,txt`: expected exit 1, Qt 2 passed/1 failed/0 skipped/0 blacklisted, 16660 ms. Actual password-account button click leaves real AccountDialog closed.
- No external browser/network, credentials, user settings, installs or live GUI; test uses offscreen/software window, private D-Bus session, synthetic private XDG/account tree, in-memory keyring, autoSync/network watcher off.
- Authorized QML repair now implemented; UI/host/accounts focused targets building at unchanged configured -j32 -l16. Candidate verification and distinct-worker review still pending.
