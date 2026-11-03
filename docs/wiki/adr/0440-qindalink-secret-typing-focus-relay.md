# ADR-0440: Type phone-held secrets into the focused window, with focus facts relayed by the shell

- **Status:** Accepted
- **Date:** 2026-11-02
- **Owners:** QindaLink desktop (`src/services/link_vault`, `link_focus`, `link_service`) and phone vault
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0433](0433-qindalink-phone-companion-architecture.md) rule 2 keeps passwords on the phone and has the
desktop type them into the window the user chose. [ADR-0439](0439-qindalink-phone-password-vault.md) provides
the phone-side steps (`VaultTypingSource`); [ADR-0436](0436-qindalink-macros-reuse-the-input-actions-library.md)
provides the RemoteDesktop portal session and the text typer; [ADR-0438](0438-qindalink-link1-caller-authority.md)
decides who may call Link1. The missing pieces are how the service learns what has focus and what it does about
a change. [ADR-0063](0063-project-authenticated-active-window-identity.md) exposes the authenticated
active-window identity only to the panel owner, which is the shell process; the service cannot read it.

## Decision

1. **Focus facts come from the shell.** The shell composition (not applet QML) joins the ADR-0063 identity
   (window uuid and the identity `revision`) with the task-list facts (application id and title) and calls
   `org.qindaqt.Link1.Focus.Publish(s json)` on every focus change, and with `{}` ("none") when the facts are
   unavailable. The JSON is at most 2048 bytes; titles are display text only, sanitized and cut to 200 code
   points, and never compared. Identity is `(uuid, revision)`.
2. **Per-member roles.** `CallerAuthority` gains `authorizeRole(message, role)`. `Focus.Publish` needs
   `CallerRole::Shell`, which is `<bindir>/qindaqt-shell` plus the test-only `--trusted-shell-caller`; the app
   (`qindaqt-link`) is a client but not a shell. Every other member keeps the client role. The coverage test
   lists `Focus.Publish` as protected, so an untrusted caller is denied by default. When the connection that
   published last leaves the bus the fact is cleared. No fact means "unknown": typing is refused with
   `error{unverified}`.
3. **Protocol.** `desktop.focus` (token for the focused window, valid at most 60 s, single use, one per phone)
   and `vault.type` (at most 16 steps; text, named key or delay), both under capability `vault`. See the
   [protocol page](../reference/qindalink-protocol-v1.md#desktop-facts-and-secret-typing-desktop-vault).
   Control characters inside text are skipped, never typed as keys: only a `{key}` step presses Tab or Enter.
4. **Typing.** The `VaultTypingFeature` uses the input lane's portal session and typer. The gate must be open.
   Focus `(uuid, revision)` is compared with the token before the job, before every step, before every 24-symbol
   batch (a batch guard in the typer) and on every focus or gate change; a mismatch aborts with
   `target-changed` after releasing held keys. One job at a time (`busy`). Keysym buffers are zeroed after use
   (`explicit_bzero`), and the plan is move-only. The JSON body's `QString`s cannot be zeroed; the guarantee
   is no persistence and no logging, as in the architecture page. If no portal session is active the job starts
   one exactly as `input.begin` does (the one-time desktop approval) and closes it afterwards unless an input
   controller is using it; `InputFeature::setPortalBorrowed` keeps the input feature from closing a session it
   did not start.
5. **Phone.** The entry detail view offers "Type username", "Type password" and "Type username + password
   (autotype)". The flow asks `desktop.focus`, shows app and title in plain text, and only after confirmation
   (which lapses after 30 s) builds the steps from `VaultTypingSource` (unlocking first when locked) and sends
   `vault.type`. A secret is never sent without that confirmation.
6. **Logging.** `vault.*` bodies are never logged on either side; the frame log prints type and key count only.

## Consequences

- The shell gains a small relay and depends on the link service only through a D-Bus call; with no shell
  running, typing is refused rather than guessed.
- The focus fact is trusted exactly as far as the shell is; a same-uid process that injects into the shell is
  out of scope (ADR-0438, "What this does not stop"). The phone's confirmation shows the shell-reported title,
  which a hostile window can influence, so the user confirms an application and window they chose.
- A short race remains between the last check and the portal delivering a batch of at most 24 symbols.
- Test-only service options `--trusted-shell-caller`, `--test-portal` and `--test-open-gate` warn on stderr.

## Revisit when

The compositor can deliver keyboard input addressed to a window id (removing the focus race), or the portal
reports the focused window itself.
