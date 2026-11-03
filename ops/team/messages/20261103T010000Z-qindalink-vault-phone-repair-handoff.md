# QindaLink lane vault-phone: repair-round handoff

Supersedes the earlier handoff for the exact commit. Merged with main; ADR renumbered to ADR-0439
(file, title, index, mkdocs, references). No device interaction this round (no adb of any kind; the
APK was built but not installed).

## Fixed (review P1)
1. Restore can no longer destroy a working vault. Only `AEADBadTagException` is "tampered"; every other
   key failure is `KEY_UNAVAILABLE` and returns to LOCKED with "Authentication failed. Try again."
   (no recovery buttons). A new vault key is created in the other Keystore slot (`-b` alias, slot in the
   authenticated header); the old key is deleted only after the new file was written, and a cancelled or
   failed create deletes the new key and leaves the old vault alone. Restore is refused from LOCKED and
   UNLOCKED in Java, and over an unusable vault it needs the explicit "restore-replace" the UI sends after
   a "this replaces the existing vault" confirmation.
2. Invalidated key no longer loops to LOCKED: a persisted `invalidated` flag that `recompute()` respects.
3. Clipboard clear: unconditional `clearPrimaryClip` after 30 s guarded by our own copy generation, and
   on lock (`VaultClipboard`).

## Fixed (P2)
Decrypted restore staging wiped on lock and idle; views unloaded when the app is not Active (secrets
dropped, next frame is list or gate); FLAG_SECURE for the whole unlocked session, re-applied when the app
returns, cleared 1.5 s after the last reason (`VaultWindowGuard`); header generation counter (AAD
covered) with highest-seen in prefs and ROLLBACK refusal; transactional import (`stageImport` /
`serializeStaged` / `commit`, applied only after the write); `Bin.Out` buffer really wiped and used for
record and backup plaintext; AtomicFile.readFully; invalidation text no longer promises fingerprint
invalidation (ADR, UI); passphrase must be at least "fair" and 12 chars or 4 words of 4+ letters, checked
on submit only; WordList SPDX is CC-BY-3.0-US with attribution in the generator panel; sensitive hints on
title, URL and search.

## Evidence
- `phone/engine-tests/run.sh`: exit 0; `VaultTests ok: 172 checks` (new: failure-class mapping with a
  wrapper that throws AEAD/invalidated/generic/provider errors, `VaultLifecycle` state rules, restore only
  when absent or unusable, key slot flags, rollback refusal, staged import untouched on failure and applied
  on commit).
- `phone/preview/run-smoke.sh`: 0 failures, 0 QML warnings; 58 grabs, backup and error states reviewed.
- `phone/tools/build-apk.sh`: built and signed (not installed). `mkdocs build --strict`: clean.

## Caveats
- Android classes (`VaultController`, `VaultKeys`, window guard, clipboard) compile but are exercised on a
  device only by the user; keystore exception types on Android 17 are unverified.
- Preview cannot simulate app-inactive; the unload-on-inactive path is untested offscreen.
- Rollback memory resets when app data is cleared.
