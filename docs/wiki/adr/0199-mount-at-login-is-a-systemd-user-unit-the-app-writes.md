# ADR-0199: Mount at login is a systemd user unit the file manager writes and nothing more

- **Status:** Proposed
- **Date:** 2026-09-17
- **Owners:** File Manager
- **Supersedes:** None
- **Superseded by:** None

## Context

A saved network location is browsable inside QindaQt's own file manager and
inside anything else that speaks KIO. Everything else on the machine — a
compiler, a video editor, a shell script — sees nothing. The conventional
answer is a FUSE mount, and the conventional trap is that a file manager grows
a mount daemon: a thing that tracks state, retries, holds credentials, and has
to be right at login before anything else runs.

QindaQt already has a service manager that does all of that: the user's own
systemd. `sshfs` is a `Type=fuse.sshfs` mount unit, `_netdev` and
`WantedBy=default.target` are its scheduling, and `systemctl --user` is its
control surface. The file manager does not need to be any of those things.

## Decision

**Mount at login is a per-location knob that writes one systemd user `.mount`
unit.** The file manager writes the file and asks the user's own service
manager to notice it. That is the whole feature.

- **It writes, it does not mount.** There is no mount daemon, no retry loop,
  no state machine, and no credential. The seam to systemd has exactly three
  operations — reload, enable, disable — because when a mount is active is
  login's business and the user's, through the ordinary `systemctl --user`
  commands they already have.
- **The unit name is the escaped mount point, computed not guessed.** systemd
  resolves a `.mount` unit's `Where=` from its own name, so a name that is not
  the path escaping yields a unit that refuses to load — or, worse, one that
  mounts somewhere the user did not choose. `MountUnit` reimplements
  `systemd-escape --path` rather than shelling out to it.
- **Ownership is a prefix, and the prefix is the rule.** The manager only ever
  reads, rewrites or removes units whose name begins with the escaping of
  `<home>/Network/`. A unit the user wrote by hand in the same directory is
  never touched. Everything mounts under `~/Network/<name>` — fixed, so the
  user can find, inspect and unmount them without QindaQt.
- **sftp only.** sshfs is the one FUSE filesystem this knob knows and the one
  that needs no root. The knob is unavailable for Windows sharing rather than
  silently ignored, and the store refuses to record it on a non-sftp location.
- **No credential, and none possible.** A saved location is userinfo-free by
  construction (ADR-0194), so the unit's `What=` is `host:/path` and sshfs
  authenticates exactly as `ssh <host>` does — `~/.ssh/config` and the agent
  (ADR-0196).
- **A location name is reduced to one safe directory component.** A separator,
  a NUL, or a control character would let a location name choose its own mount
  point anywhere in the tree.
- **Idempotent.** An unchanged unit is not rewritten and systemd is not asked
  to reload, so opening a window does not churn the service manager.

Turning the knob on is what makes `net-fs/sshfs` a runtime dependency — for
that user, on that machine, and nowhere else. The file manager does not
install it and cannot; a missing sshfs shows up as a failed mount in the
user's journal, and the Preferences copy says so plainly.

The saved-location inventory becomes `network-locations-v2` to carry the knob.
A v1 inventory is read once, reported as migrated, and rewritten as v2 with
the knob off — nobody is opted into a mount they never asked for. The v1 file
is left where it is, so downgrading to an older build loses nothing.

## Consequences

- No new QindaQt process, no polling, and no code in QindaQt that has ever
  seen a password.
- `net-fs/sshfs` must be installed for the knob to do anything. On
  `qinda-top` it is not installed today; that is packaging's and the Program
  Manager's call, not this lane's, and nothing in QindaQt installs it.
- Focused rows: `qindaqt.file-manager-mount-unit` (escaping and unit text,
  each escaping case checked against `systemd-escape --path`) and
  `qindaqt.file-manager-mount-manager` (write, enable, retire, idempotence,
  and the ownership prefix). **No row ever runs `systemctl`** — one that did
  would touch the developer's own live user manager.
- Because no row runs systemctl and sshfs is absent on the target machine, an
  actual mount at login has **not** been observed. That proof needs an install
  and a login, and belongs to whoever cuts the package.

## Revisit when

A user needs to mount something that is not sftp, needs a mount point outside
`~/Network`, or needs the file manager to show whether a mount is currently
active — the last of which would mean subscribing to systemd's unit state and
is deliberately not part of this decision.
