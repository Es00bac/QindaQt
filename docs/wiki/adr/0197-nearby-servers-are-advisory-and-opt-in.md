# ADR-0197: Nearby servers are advisory, opt-in, and only what can be opened

- **Status:** Proposed
- **Date:** 2026-09-17
- **Owners:** File Manager
- **Supersedes:** None
- **Superseded by:** None

## Context

ADR-0194 made a network location something the file manager remembers, but a
user still had to know the address before they could save it. Both machines
already run `avahi-daemon`, and both already advertise themselves: browsing
`_sftp-ssh._tcp` on this network returns `qinda-14` on `wlan0`, `tailscale0`
and `lo`, once per protocol family — ten announcements for one machine.

Three things about that shape the decision. Discovery is a **network
activity**: a file manager that starts probing the local network the moment it
opens has made a choice for the user. Announcements are **not identities**: the
same machine appears many times, and two machines may advertise the same name.
And an announcement is **not an invitation**: nothing about it says the server
will accept a connection.

## Decision

- **Opt-in.** Browsing runs only while the `discoverNearbyServers` preference
  (ADR-0198) is on, and it is off by default. Opening the Network hub does not
  start it; the preference does. The hub says so rather than showing an empty
  list that looks broken.
- **Only what can be opened.** `_sftp-ssh._tcp` and `_ssh._tcp` become `sftp`
  (KIO's sftp worker speaks SSH); `_smb._tcp` becomes `smb`. Nothing else is
  browsed. **NFS is deliberately absent** even though the wave plan listed it:
  the ADR-0137 allowlist cannot open an NFS location, and a row that cannot be
  opened is worse than no row.
- **One machine is one row.** The published identity is the canonical
  `scheme://host[:port]` that `NetworkLocation::canonicalize()` accepts — so a
  hostile advertisement cannot introduce an address the rest of the file
  manager would later refuse — and announcements are reference-counted against
  it, so a machine disappears only when its last interface withdraws it. A
  default port is not carried, which keeps the address free of a redundant
  `:22`.
- **Advisory, never trusted.** Opening a discovered server hands its address
  to `NavigationController::navigateTo()`, exactly as a saved location does.
  The same allowlist, the same navigation state pane on failure, the same
  platform credential prompt (ADR-0196). Discovery authenticates nothing,
  connects to nothing, and marks nothing as reachable.
- **Silence is reported.** A bus that cannot be reached, or a browser that
  fails, is published as an unavailable-reason, because silence is
  indistinguishable from "there is nothing here".
- **A missing provider is a supported composition.** With no discovery backend
  injected, `supported` is false and the Nearby section is hidden; every other
  surface is unaffected.

The Avahi client is an injected `ServiceDiscovery`, and its four D-Bus edges
(browser creation, resolve, subscribe, release) are overridable seams, so the
whole state machine is testable without a bus or a daemon.

## Consequences

- No new dependency: Avahi is already installed and running on both machines,
  and is reached on the system bus where it lives.
- A user who never turns discovery on pays nothing — no bus traffic, no
  timers, no browsers.
- `qindaqt.file-manager-avahi-discovery` proves the browse/resolve/collapse/
  retire state machine with canned replies;
  `qindaqt.file-manager-discovery-controller` proves the visible model.
  Neither touches a bus, a daemon, or a network.
- The wave plan's fourth service type (`_nfs._tcp`) is not implemented, and
  the wiki says why.

## Revisit when

QindaQt gains a way to open NFS (which would mean an ADR-0137 successor), or
discovery needs to survive being left on for hours — at which point the
browser will want a cache-exhausted/all-for-now signal and a periodic refresh
that this decision deliberately does not include.
