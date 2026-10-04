# Office native imported Password bridge — implementation finding

- Time: 2026-10-04T17:44:00+00:00
- Worker: genuine collaboration agent `/root/native_permission_repair`
- Exact base: `700d85b7ba73aa6a19d06bdaa3d8512c83c734e6`
- Worktree: `/home/cabewse/work_space/QindaOffice-native-secret-bridge-20261004`

KDE's [Entry source](https://raw.githubusercontent.com/KDE/kwallet/master/src/runtime/kwalletbackend/kwalletentry.cc) writes and reads Password as a QDataStream QString. The client decoder now pins big-endian Qt6.0 compatible bounded QString framing, validates declared length before allocation, and rejects unsupported null/empty/NUL/malformed/trailing input. No raw byte relabelling or KWallet activation.

Source now separates typed codec, primary-first backend composition, private standard wire types, and an asynchronous native Secret Service reader. Exact imported folder/key/type1 match, same native/standard name owner pin, post-unlock decrypted attributes, one-match recheck, bounded calls/prompt and owned cleanup remain explicit. Only NotFound falls back; native failures remain native failures. New writes stay primary; manager approved failure after successful ordinary remove if a preserved import still exists.

Tests will use a synthetic provider on an isolated bus only. No live app, credentials, keyring unlock, original record mutation, installed software, settings or desktop change.
