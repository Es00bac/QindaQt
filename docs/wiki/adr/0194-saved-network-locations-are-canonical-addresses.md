# ADR-0194: A saved network location is a name and a canonical address

- **Status:** Proposed
- **Date:** 2026-09-17
- **Owners:** File Manager
- **Supersedes:** None
- **Superseded by:** None

## Context

ADR-0137 made `smb://` and `sftp://` browsable, and ADR-0152 through ADR-0157
made a remote folder openable, renameable, and writable in place. Nothing
remembered a server between launches. The Network place opened the location
bar, so using the desktop's own file manager for the two machines' shares
meant retyping `sftp://qinda/mnt/storage` every session, and the Places
sidebar — the one surface a file manager is expected to keep folders in —
knew only local paths.

Two constraints shape what a remembered location may hold.

`NetworkLocation::canonicalize()` is the single allowlist for every address
that is browsed: lower-cased `smb`/`sftp` only, a non-empty host, no `.` or
`..` segment, and **no userinfo at all**. Its guard exists so a pasted
`smb://user:pass@host/share` is refused outright rather than silently
connecting anonymously with the credential dropped. An inventory that stored
a different, wider address form would be a second, weaker gate on the same
authority.

`docs/wiki/architecture/secret-service.md` is equally explicit: gnome-keyring
is the session's only Secret Service provider and "QindaQt code never reads
or writes collection contents". Anything a saved location holds is therefore
not a secret, and must not need to be one.

## Decision

A saved network location is **a display name, a canonical address, and a flag
for whether it appears in the Places sidebar** — nothing else.

- The inventory lives in `network-locations-v1.json` beside `bookmarks-v1.json`
  under `$XDG_STATE_HOME/qindaqt-file-manager`, written through the same
  symlink-refusing, size-bounded, atomically committed `StateFile` primitive
  the bookmark inventory uses (ADR-0090). At most 64 locations, at most 64 KiB.
- A record's identity **is** its canonical address, so saving the same folder
  twice updates one card instead of making two.
- Every address that enters or leaves the store must survive
  `NetworkLocation::canonicalize()` unchanged. A record that does not makes
  the whole inventory `Malformed`: a partly-loaded inventory would quietly
  lose a location the user saved.
- The reader demands an exact key set, both for the document and for each
  entry. An inventory written by a newer schema is refused rather than
  half-understood; adding a field means `network-locations-v2` and a
  migration, not a tolerant reader.
- The Network place opens a **hub page** — the saved locations, a
  Connect-to-server dialog, and a plain statement of how signing in works —
  rather than the location bar. Activating a location hands its address to
  `NavigationController::navigateTo()`, exactly as a bookmark hands over a
  path, so an unreachable server lands on the ordinary navigation state pane
  and not on a second error channel.
- Turning dialog text into a record is one pure function,
  `buildNetworkLocation()`: no I/O, no host resolution, no environment. Every
  refusal it returns is shown verbatim, so the dialog validates nothing itself
  and cannot disagree with the store.

The dialog deliberately has **no user-name, password, or credential-source
field**, and the record has no per-location user name. Sign-in is the
platform's (ADR-0196). A per-location user name would put userinfo in a
browsed address and is deferred to a schema bump with its own ADR.

## Consequences

- The laptop's `sftp://qinda/mnt/storage` and `sftp://qinda/home/cabewse`
  survive a restart and appear in the sidebar. They work today because KIO's
  sftp worker uses libssh, which honours `~/.ssh/config` and the ssh agent.
- Saving a location is the only way the file manager writes a network
  address to disk, and that address can never contain a credential.
- The action catalog gains `go.network` (Alt+N) and `network.connect`
  (Ctrl+Shift+S); its pinned size moves from 29 to 31.
- SMB shares that need a distinct user name must rely on the platform's
  credential prompt until the schema carries one.
- Focused rows: `qindaqt.file-manager-network-locations-store`,
  `qindaqt.file-manager-connect-request`,
  `qindaqt.file-manager-network-locations-controller`. The installed-package
  probe's contract list gains the hub, the dialog, and their fields.

## Revisit when

A location needs to carry something a name and an address cannot express —
a distinct remote user name, a mount-at-login unit, or an in-place versus
copy-on-open choice. Each of those is a `network-locations-v2` field and a
migration, and a remote user name additionally needs the ADR-0137 no-userinfo
guard re-examined rather than quietly relaxed.
