# ADR-0135: gnome-keyring is the Secret Service provider

- **Status:** Accepted
- **Date:** 2026-09-11
- **Owners:** Session, Platform integration
- **Supersedes:** None
- **Superseded by:** None

## Context

Applications on a Linux desktop keep passwords, tokens, and certificates in a
secret store behind the freedesktop Secret Service API
(`org.freedesktop.secrets`). Without a provider, Chromium-based applications
fall back to a hardcoded-plaintext encryption key, and libsecret clients fail.
QindaQt ships no secret store of its own and must adopt one, not invent one.

On this machine gnome-keyring 48.0 owns `org.freedesktop.secrets` and
`org.gnome.keyring` on the session bus: D-Bus starts it on demand through
`/usr/share/dbus-1/services/org.freedesktop.secrets.service`, which
gnome-keyring itself owns and installs. A `login` collection already exists,
is the default alias, and is unlocked at login by PAM
(`gkr-pam: unlocked login keyring` in the journal). KWallet 6.27 is installed
but unused: `pam_kwallet_init` never runs because QindaQt does not start
`graphical-session.target`, `~/.local/share/kwalletd` is empty, and no KDE
wallet collection exists to adopt. ksecretd ships alongside it and owns no
activation name. The session supervisor starts a fixed list of children and
must not learn a keyring child; on-demand D-Bus activation is the correct
lifecycle for a store that must outlive nothing and start only when used.

The remaining problem is application-side: how Chromium-based applications
choose a store when `XDG_CURRENT_DESKTOP=QindaQt`, and what the session owes
them. Neither Chromium nor Electron honours any environment variable for this
choice; they honour the `--password-store` switch and the desktop name.
Appending `:GNOME` to `XDG_CURRENT_DESKTOP` would change portal, GTK, and Qt
desktop matching and is rejected. Firefox and Thunderbird are unaffected: they
encrypt `logins.json` under NSS `key4.db` and never consult the desktop name
(Firefox 155 has no OS-keyring module).

Verified selection behavior of the installed engines:

| Engine generation | Unknown desktop name | Override |
| --- | --- | --- |
| Chromium os_crypt sync (Electron 41, Chrome 146) | `basic` backend: key wrapped with the hardcoded password `peanuts` — plaintext-equivalent | `--password-store=gnome-libsecret` |
| Chromium os_crypt async (Chromium 152) | Secret Service (`org.freedesktop.secrets`) | `--password-store=basic` downgrades |
| Firefox 155, Thunderbird 155 | own NSS storage | none needed |

Applications using Electron's synchronous `safeStorage` API keep the
plaintext-equivalent `basic_text` semantics on an unrecognised desktop name;
the asynchronous API and current Chromium select the Secret Service. The
installed ChatGPT (Chromium 152, async) is already correct; claude-desktop
(Electron 44), ZCode, and sloom-studio (Electron 41, synchronous safeStorage)
default to `basic_text` under `XDG_CURRENT_DESKTOP=QindaQt` unless launched
with `--password-store=gnome-libsecret`. ZCode additionally carries an
application-side `--password-store=basic` pin that only its vendor can lift.

## Decision

1. **gnome-keyring's secrets component is QindaQt's Secret Service
   provider.** The session keeps `XDG_CURRENT_DESKTOP=QindaQt`, starts no
   keyring process, and relies on D-Bus activation of
   `org.freedesktop.secrets`. The daemon runs as
   `gnome-keyring-daemon --start --foreground --components=secrets`; the
   `login` collection is the default alias. QindaQt code never reads or writes
   collection contents; ownership of secrets stays with the applications and
   the user.
2. **KWallet and ksecretd are not used.** There is no KWallet collection to
   adopt, its PAM unlock never fires in a QindaQt session, and gnome-keyring
   owns the Secret Service activation file, so both providers would fight over
   `org.freedesktop.secrets`. Applications that choose a store from the
   desktop name are pointed at the keyring with
   `--password-store=gnome-libsecret` (documented flags; per-application
   launcher overrides are a packaging concern, not session behavior).
3. **Login unlock contract (distribution requirement).** A QindaQt
   distribution must ship, in the display manager's PAM stack (for example
   `/etc/pam.d/sddm`): `-auth optional pam_gnome_keyring.so`, the
   `password` stack line with `use_authtok`, and `session optional
   pam_gnome_keyring.so auto_start`. With those lines the login keyring is
   unlocked with the login password before the session starts; the journal
   records `gkr-pam: unlocked login keyring`. A stack without them leaves the
   `login` collection locked after login; applications then prompt or fail.
   QindaQt does not manage PAM files.
4. **Locked-keyring prompts go through gcr-prompter.** When an operation
   needs an unlocked collection and it is locked, gnome-keyring creates a
   Secret Service prompt object and asks `org.gnome.keyring.SystemPrompter`
   for a prompter; `/usr/libexec/gcr-prompter` (gcr 4) answers and shows a
   dialog on the live Wayland session. A session without a prompter fails the
   prompt and the operation errors out; it must not hang. Tests and tools
   therefore never rely on prompting.
5. **SSH agent: out of scope.** gnome-keyring 48 no longer ships an SSH
   agent. gcr-ssh-agent is installed on this machine
   (`/usr/libexec/gcr-ssh-agent` with `gcr-ssh-agent.socket` and
   `gcr-ssh-agent.service` user units). Enabling it is a user choice; QindaQt
   sets no `SSH_AUTH_SOCK` and starts no agent this wave.
6. **Verifiability.** The provider contract is pinned by a private-bus test
   (`tests/services/secret_service/`): the daemon owns
   `org.freedesktop.secrets`, the default alias resolves to an unlocked
   collection, a secret round-trips through the Secret Service API, a locked
   collection yields nothing without prompting, a wrong unlock password leaves
   the collection locked, and a client without the daemon fails closed within
   a bounded time. `tools/keyring-check/qindaqt-keyring-check` reports the
   live provider, default-collection lock state, PAM lines, and portal Secret
   routing read-only.

## Consequences

- Passwords in Chromium-based, libsecret, and Flatpak applications stay in the
  unlocked `login` keyring instead of plaintext-equivalent `basic` storage,
  provided the application is async-era, or launched with
  `--password-store=gnome-libsecret`, or sandboxed behind the Secret portal
  (routed by the portal lane).
- Users who relied on `basic` storage see a one-time re-entry of saved
  credentials when an application switches backends; Chromium migrates the v10
  key on restart. This is accepted.
- The distribution PAM requirement is documented in the handbook; absence of
  the PAM lines is a reported finding of `qindaqt-keyring-check`, not a
  session failure.
- The session supervisor, portal routing, and packaging files stay untouched;
  the launcher-flag overrides for installed third-party applications are
  requested from the Program Manager in the lane handoff.
- KWallet remains installed but inert; if a future KDE application port needs
  it, a new ADR must reconcile the two providers.

## Revisit when

- Chromium or Electron honours an environment variable for store selection, or
  makes the async Secret Service default universal, so the documented flags
  become unnecessary.
- gnome-keyring is unavailable on a target distribution and ksecretd or another
  Secret Service implementation must be adopted instead.
- Firefox or Thunderbird ships Linux OS-keyring integration.
