# ADR-0439: QindaLink phone password vault

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
- **Keys.** A random 256-bit DEK is wrapped by a Keystore AES-256-GCM key
  (`qindalink-vault-kek`, with `-b` as the second slot): user authentication for
  every use (timeout 0, class-3 biometric or device credential), unlocked
  device required, StrongBox with TEE fallback (recorded in the header flags
  together with the slot). Unlock uses the framework `BiometricPrompt` with a
  `CryptoObject`. Invalidation by biometric enrollment is requested, but since
  the device credential is an allowed authenticator Android may not invalidate
  the key when a fingerprint is added; the app therefore only promises
  invalidation when the screen lock is removed or reset.
- **Failure classes.** Only `AEADBadTagException` from the unwrap means
  tampered. A key that cannot be used right now (not authenticated, device
  locked, Keystore busy) is `KEY_UNAVAILABLE`: the vault returns to locked with
  "Authentication failed. Try again." and no recovery buttons. A permanently
  invalidated key sets a persisted `invalidated` flag (the stale key can still
  exist) and offers restore or erase. Rules live in `VaultLifecycle` and are
  unit tested.
- **Rollback.** The header carries a u64 generation (covered by the index
  AAD). The phone remembers the highest generation it has opened and refuses an
  older file with a clear message.
- **Session.** The DEK is a `byte[]` zeroed on lock. Locks: 5 min idle, screen
  off, 30 s without UI, explicit. Passwords are decrypted per request and sent
  to QML only on an explicit reveal; QML drops them on hide, after 10 s and
  when the view is destroyed. Residual risk: Java `String`/`QString` copies of a
  revealed password and the provider's internal copy of the key cannot be
  zeroed.
- **Restore and replace.** Restore is refused from a locked or open vault and
  is offered only when the vault is absent or known unusable. Replacing an
  existing vault needs an explicit "this replaces the existing vault"
  confirmation. The new key is created in the other Keystore slot; the old key
  is deleted only after the new file was written, so a failed or cancelled
  restore leaves the old vault untouched. Decrypted restore staging is wiped on
  lock or idle. Imports are transactional: the new state is built and written
  first and applied to the session only after the write succeeded.
- **Backup.** PBKDF2-HMAC-SHA256 (at least 600 000 iterations, calibrated to
  about 600 ms on the device and stored in the header), 16-byte salt, AES-GCM,
  header authenticated as AAD. Passphrase: at least 12 characters or 4 words.
  Import merges (newer entry wins) or replaces after confirmation. No plaintext
  export exists. Files go through the Storage Access Framework from an
  invisible helper activity.
- **UI hardening.** `FLAG_SECURE` is on while the vault page is shown and for
  the whole unlocked session (re-applied when the app returns to the front, and
  cleared a moment after the last reason ends), plus the helper activity. Secret
  fields and the entry title, URL and search fields use sensitive-data,
  no-prediction hints. When the app leaves the foreground the vault views are
  unloaded so revealed or typed secrets are dropped. Clipboard copy is off by
  default, marks the clip sensitive and is cleared unconditionally 30 s later and
  on lock (Android 10+ does not let a background app read the clip to compare).
- **Backup passphrase.** At least "fair" (40 bits estimated) and at least 12
  characters or 4 words of 4+ letters; it is evaluated on submit, not per
  keystroke.
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
- Whole-file rollback is detected only against the highest generation this phone has seen; clearing app data resets that memory.
- Part 2 must not add logging of step contents and must close sources promptly.
