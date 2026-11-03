# ADR-0438: QindaLink phone password vault

- **Status:** Accepted
- **Date:** 2026-11-02
- **Owners:** Shell
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0433](0433-qindalink-phone-companion-architecture.md) rule 2 keeps the
password vault on the phone only. Part 1 (this ADR) builds the vault, the
unlock, the encrypted backup and the seam for desktop typing; part 2 will add
the `vault.*` and `desktop.*` frames.

## Decision

- **Format** (`org.qindaqt.link.vault`, pure Java, JVM tested). One file with a
  header, an encrypted index and one AES-256-GCM record per entry, each with
  its own random 96-bit IV. The AAD of an entry is its id and the format
  version. The whole file fails closed on any changed byte (tested exhaustively
  by flipping every byte).
- **Deviation from the lane brief.** The index AAD also covers the exact header
  bytes (version, flags, wrapped key), so a flipped header byte fails the tag
  instead of being trusted. Entries are additionally checked against the index
  (id, title, username, URL, timestamp) when decrypted.
- **Keys.** A random 256-bit DEK is wrapped by the Keystore AES-256-GCM key
  `qindalink-vault-kek`: user authentication for every use (timeout 0, class-3
  biometric or device credential), invalidated by biometric enrollment,
  unlocked device required, StrongBox with TEE fallback (recorded in the header
  flags). Unlock uses the framework `BiometricPrompt` with a `CryptoObject`.
- **Session.** The DEK is a `byte[]` zeroed on lock. Locks: 5 min idle, screen
  off, 30 s without UI, explicit. Passwords are decrypted per request and sent
  to QML only on an explicit reveal; QML drops them on hide, after 10 s and
  when the view is destroyed. Residual risk: Java `String`/`QString` copies of a
  revealed password and the provider's internal copy of the key cannot be
  zeroed.
- **Invalidated key.** The vault cannot be opened; the UI says so and offers
  restore from backup or erasing the vault.
- **Backup.** PBKDF2-HMAC-SHA256 (at least 600 000 iterations, calibrated to
  about 600 ms on the device and stored in the header), 16-byte salt, AES-GCM,
  header authenticated as AAD. Passphrase: at least 12 characters or 4 words.
  Import merges (newer entry wins) or replaces after confirmation. No plaintext
  export exists. Files go through the Storage Access Framework from an
  invisible helper activity.
- **UI hardening.** `FLAG_SECURE` follows the vault page (and the helper
  activity); secret fields use sensitive-data, no-prediction hints; clipboard
  copy is off by default, marks the clip sensitive and clears it after 30 s.
- **Generator.** `SecureRandom`; 8-128 characters with class guarantees, or
  3-12 words from the EFF large word list (7776 words, Creative Commons
  Attribution 3.0 US, (c) Electronic Frontier Foundation, generated into
  `WordList.java`).
- **Part 2 seam.** `VaultTypingSource` yields text/key/delay steps only while
  unlocked and zeroes them on close or lock. Autotype grammar:
  `{USERNAME} {PASSWORD} {URL} {TAB} {ENTER} {KEY name} {DELAY ms}`, `{{}` and
  `{}}` for braces; default `{USERNAME}{TAB}{PASSWORD}{ENTER}`.

## Consequences

- No Android class leaks into the format package; Keystore behaviour is only
  provable on a device.
- Whole-file rollback to an older valid vault file is not detected.
- Part 2 must not add logging of step contents and must close sources promptly.
