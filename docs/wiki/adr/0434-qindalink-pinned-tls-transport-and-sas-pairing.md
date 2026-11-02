# ADR-0434: QindaLink pinned TLS transport and code-comparison pairing

- **Status:** Accepted
- **Date:** 2026-11-02
- **Owners:** QindaLink (Platform workgroup)
- **Supersedes:** None
- **Superseded by:** None

## Context

The QindaLink link ([ADR-0433](0433-qindalink-phone-companion-architecture.md))
carries SMS, notifications, input and vault secrets. Two transports have to
work:

- **USB.** The desktop runs `adb reverse`, and the phone connects to
  `127.0.0.1` on its own side. Any app on the phone can reach that loopback
  port, and any local process on the desktop can reach the desktop listener.
  ADB's own authentication therefore does not authenticate the phone app.
- **LAN or Tailscale.** The connection crosses a network that is not trusted.

Qt does not expose TLS exporter keying material from `QSslSocket`, so the
pairing check cannot rely on channel binding. A six-digit code computed only
from the two public keys can be ground by a man-in-the-middle in about 10⁶
key generations.

## Decision

1. **One transport for every path.** Every link connection is TCP with TLS 1.3
   only, and both sides present certificates. USB uses the same TLS session
   over the ADB reverse tunnel, and ADB adds nothing to authentication.
2. **Identities are pinned keys.** Each side has one long-lived ECDSA P-256
   key with a self-signed certificate. Its identity is
   `SHA-256(DER SubjectPublicKeyInfo)`, and certificate chains, names and
   expiry are ignored.
   - **Phone:** the key is generated in Android Keystore (TEE, not StrongBox,
     so handshakes stay fast). It is non-exportable and does not require user
     authentication.
   - **Desktop:** the key is a PEM file in `$XDG_DATA_HOME/qindalink/identity/`
     (directory 0700, key 0600), generated with OpenSSL's libcrypto, the same
     model as an SSH host key. It does not use the keyring, so a background
     reconnect never raises a keyring prompt.
3. **Paired peers only, outside pairing.** A connection whose peer key is not
   in the trust store may exchange only `hello`, `pair.*`, `ping`, `pong`,
   `error` and `bye`. Any other frame closes it.
4. **Pairing compares a code with a commitment.** The desktop commits to a
   random nonce before it sees the phone's nonce. Both sides then derive a
   six-digit code from both keys and both nonces. The user confirms that the
   codes match on **both** screens, and the pin is stored only after both
   sides accept. The exact frames and hash inputs are normative in the
   [protocol reference](../reference/qindalink-protocol-v1.md#pairing).
5. **When pairing is accepted.**
   - Over the loopback listener (USB), the desktop accepts a pairing request
     at any time and presents it as a desktop prompt.
   - Over the LAN listener, it accepts a request only while the user has
     opened a pairing window in the QindaLink app. The window closes after
     120 seconds.
6. **Listeners.**
   - The loopback listener on `127.0.0.1:47160` is always on while the service
     runs.
   - The LAN listener (same port on non-loopback interfaces, advertised as
     `_qindalink._tcp` through Avahi) is off by default and enabled in
     settings.
7. **Unpairing is local and immediate.** Removing a peer from either trust
   store drops its connection. The other side learns of it through `bye` with
   reason `unpaired` if it is connected, otherwise on its next connection
   attempt.

## Consequences

- A man-in-the-middle has to win a one-in-a-million code match per pairing
  attempt and cannot grind keys offline. Each failed attempt is visible to the
  user as a mismatched code.
- The desktop service gains a direct `libcrypto` dependency for key and
  certificate generation. TLS itself stays in `QSslSocket`/`QSslServer`.
- The desktop identity key is protected only by file permissions, so a process
  running as the user can impersonate the desktop to the phone. That matches
  the trust boundary of every other per-user QindaQt service.
- Unit tests need throwaway identities and a test trust store. They must never
  touch the real `$XDG_DATA_HOME/qindalink`.

## Revisit when

- Qt exposes TLS exporters, which would allow channel-bound pairing without a
  commitment round.
- A second desktop user or a multi-user phone requires per-profile identities.
