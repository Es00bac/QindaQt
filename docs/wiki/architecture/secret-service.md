# Secret Service provider

QindaQt ships no secret store of its own. Applications that follow the
freedesktop Secret Service API (`org.freedesktop.secrets`) — libsecret
clients, Chromium-based browsers and Electron applications, and Flatpak
applications through the Secret portal — store their passwords in
gnome-keyring's `login` collection. The decision and the alternatives it
rejects are recorded in [ADR-0135](../adr/0135-gnome-keyring-secret-service.md).

## Provider contract

- gnome-keyring's secrets component is the only Secret Service provider. D-Bus
  starts it on demand (`gnome-keyring-daemon --start --foreground
  --components=secrets`) through the `org.freedesktop.secrets.service`
  activation file that gnome-keyring owns. The session supervisor starts no
  keyring child and QindaQt code never reads or writes collection contents.
- The `login` collection is the default alias. Clients that resolve
  `/org/freedesktop/secrets/aliases/default` receive it.
- KWallet and ksecretd stay unused: no wallet collection exists to adopt, its
  PAM unlock never fires in a QindaQt session, and gnome-keyring owns the
  activation name both would claim.

The session environment exports `XDG_CURRENT_DESKTOP=QindaQt`
(`src/session`). The name is otherwise meaningless to secret-store selection;
nothing else about the desktop changes.

## Login unlock

A distribution image for QindaQt must ship these `pam_gnome_keyring.so` lines
in the display manager PAM stack (for example `/etc/pam.d/sddm`):

| Stack | Line |
| --- | --- |
| `auth` | `-auth optional pam_gnome_keyring.so` |
| `password` | `password optional pam_gnome_keyring.so use_authtok` |
| `session` | `session optional pam_gnome_keyring.so auto_start` |

With them the login keyring is unlocked with the login password before the
session starts and the journal records `gkr-pam: unlocked login keyring`.
Without them the collection stays locked after login and every client either
prompts or fails. QindaQt itself does not manage PAM files.

## Locked collections and prompting

Reading a secret from a locked collection never prompts; the client sees an
empty result or a locked error. Operations that need to unlock a collection
create a Secret Service prompt object and ask `org.gnome.keyring.SystemPrompter`
for a prompter. `gcr-prompter` (gcr 4, `/usr/libexec/gcr-prompter`) answers on
the live Wayland session with a dialog. A session without a prompter fails the
operation; it must not hang. Tests must not depend on prompting.

## Application store selection

Chromium-based engines pick a safe-storage backend from an explicit
`--password-store` switch, else from the desktop name. An unrecognised
`XDG_CURRENT_DESKTOP` value yields plaintext-equivalent `basic` storage
(key wrapped with the hardcoded password `peanuts`) on os_crypt sync-era
engines; async-era engines use the Secret Service instead. No environment
variable can steer this choice, and redefining `XDG_CURRENT_DESKTOP` is
rejected because portals, GTK, and Qt match against the same value.

| Application class | Under `XDG_CURRENT_DESKTOP=QindaQt` | Requirement |
| --- | --- | --- |
| Chromium os_crypt async (Chromium 152+) | Secret Service | none |
| Chromium os_crypt sync (Electron 41 and older) | `basic` unless overridden | launch with `--password-store=gnome-libsecret` |
| libsecret clients, GNOME applications | Secret Service | none |
| Firefox, Thunderbird | own NSS storage (`key4.db`) | none |
| Flatpak applications | Secret portal (routed to gnome-keyring) | none |

Older engines also accept the `--password-store=gnome` token; scripts should
prefer the explicit `gnome-libsecret` value. Launcher overrides for installed
third-party applications belong to packaging and are tracked as a Program
Manager request, not as session behavior.

## SSH agent

gnome-keyring 48 no longer ships an SSH agent. `gcr-ssh-agent` provides one
(`/usr/libexec/gcr-ssh-agent` with `gcr-ssh-agent.socket` and
`gcr-ssh-agent.service` user units). Enabling it is a user choice; QindaQt
sets no `SSH_AUTH_SOCK` and starts no agent.

## Verification

- `tests/services/secret_service/` proves the provider contract on a private
  bus: name ownership, default alias resolving to an unlocked collection, a
  secret round-trip through `secret-tool`, a locked collection yielding
  nothing without prompting, a wrong unlock password leaving the collection
  locked, and a bounded fail-closed client error without a daemon. Registered
  as `qindaqt.secret-service-...` ctest rows.
- `tools/keyring-check/qindaqt-keyring-check` reports, read-only, who owns
  `org.freedesktop.secrets`, whether the default collection is unlocked, the
  presence of the PAM lines, and whether the portal Secret row is routed
  (`src/services/portal` owns the routing file).

Network credentials stay separate: the [network secret agent](network-secret-agent.md)
prompts for Wi-Fi passwords and hands them to NetworkManager, which stores
remembered credentials in the same keyring through the Secret Service API.
User-facing guidance lives in the handbook under
[Passwords and keys](../handbook/privacy.md#passwords-and-keys).
