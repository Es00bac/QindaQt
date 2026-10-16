# ADR-0387: Resolve privileged desktop entries by basename and local paths only

- **Status:** Accepted — fork source candidate; installed delivery pending
- **Date:** 2026-10-16
- **Owners:** Compositor fork
- **Amends:** [ADR-0340](0340-use-native-privileged-compositor-identities.md)
- **Superseded by:** None

## Context

To decide whether a client may bind a restricted Wayland global or call
Screenshot2, the compositor maps the client's executable to the application
entry whose `Exec` resolves to it, then reads that entry's
`X-QindaQt-KWin-*` permission lists (ADR-0340). The inherited KWin code
canonicalized the `Exec` path of every installed application on the compositor
thread, for each new client. One of the owner's entries points through a
symlink into an sshfs automount; the compositor triggered that automount 184
times in four days, stalling about a second each time and up to the 20 s mount
timeout when the remote machine was unreachable (stability findings item 5).

## Decision

- An entry is a candidate only when its `Exec` basename equals the client
  executable's basename: a string comparison that touches no file system.
- Only candidates are canonicalized, one path component at a time. A component
  on a network, FUSE or autofs mount, read from `/proc/self/mountinfo`, is
  refused before it is looked up, so automount points are never triggered and
  no remote peer or userspace daemon is ever waited on. A remote root file
  system is exempt, because the compositor already depends on it.
- The answer is cached per canonical client executable until KSycoca reports a
  new service database; answers computed while KSycoca may still serve the
  previous database are returned but not kept.

A privileged entry must therefore name the real executable stored on a local
file system. An `Exec` symlink whose basename differs from its target, or an
executable on a network/FUSE/autofs mount, no longer matches. All QindaQt
entries that carry permission keys already name their real `/usr` binaries.
Launch-time trust checks for the locker and capture children are unchanged.

## Consequences

- No permission lookup can mount a share or block on one.
- Each client costs a string scan of the service list once, then a hash lookup.
- The fork's `qindaqt-permission-unreachable-exec` test mounts a FUSE file
  system that counts requests in a private user/mount namespace and points
  entries into it through a symlink; the lookup must send it no request, while
  plain `realpath(3)` of the same `Exec` path must reach it. The service lookup
  test covers caching, invalidation and the renamed-symlink refusal.

## Revisit when

A privileged client legitimately needs an `Exec` alias or a network-hosted
binary, or KSycoca exposes the identity of the database it has loaded.
