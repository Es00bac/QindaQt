# QindaLink lane vault-phone: handoff (part 1 of 2)

- Branch `link/vault-phone`, merged with main at `eb2b67e88` (conflict only in `phone/engine-tests/run.sh`, resolved by keeping both lists).
- Design: ADR-0439 and the "Phone vault (part 1)" section of `docs/wiki/architecture/qindalink.md`.

## Changed paths
- `phone/app/android/src/org/qindaqt/link/vault/**` (pure Java: format, session, backup, generator, autotype, typing seam, generated EFF `WordList.java`).
- `phone/app/android/src/org/qindaqt/link/android/Vault{Keys,Controller,BackupFlow,BackupActivity,Bridge}.java`, manifest activity entry.
- `phone/app/VaultBridge.{h,cpp}`, `main.cpp`, `CMakeLists.txt`; `phone/app/qml/Vault*.qml`, `qml-files.cmake`, additive lines in `Main.qml`.
- `phone/preview/FakeVault.*`, `main.cpp` (vault grabs and FLAG_SECURE check), `CMakeLists.txt`.
- `phone/engine-tests/VaultTests.java`, `run.sh`; docs, `mkdocs.yml`, ADR index.

## Evidence
- `phone/engine-tests/run.sh`: exit 0; `VaultTests ok: 125 checks` (every single-byte flip of the vault file rejected; backup tamper, wrong passphrase, downgraded iteration count, generator, autotype grammar, typing source locked/closed zeroing).
- `phone/preview/run-smoke.sh`: 0 failures, 0 QML warnings (run before the last main merge); grabs of all 14 vault states and views reviewed.
- `phone/tools/build-apk.sh` builds and signs with the shared key; APK installed on the phone (`install --user 0 -r`, Success).
- `mkdocs build --strict`: clean.

## Not done (bounded caveats)
- Device checks incomplete. The phone was in use by the user (another app was in front) when I started UI driving, so I stopped. No FLAG_SECURE screencap proof, no "biometric prompt shown" proof, no live unlock. One accidental screencap showed another app's personal content; it was deleted immediately and never read into any file. One tap (875,2245) landed in that app's navigation area; nothing was typed or sent.
- Keystore behavior (StrongBox fallback, BiometricPrompt with CryptoObject plus device credential on Android 17) is unverified on the device.
- Whole-file rollback to an older valid vault is not detected.
- Preview smoke was not re-run after the final main merge.

## Requested next action
The user unlocks the phone, opens QindaLink, Tools, Vault, creates a vault with a throwaway entry, and checks that a screenshot of the vault page is blank.
