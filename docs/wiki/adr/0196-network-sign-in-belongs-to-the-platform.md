# ADR-0196: Network sign-in belongs to the platform, not to QindaQt

- **Status:** Proposed
- **Date:** 2026-09-17
- **Owners:** File Manager
- **Supersedes:** None
- **Superseded by:** None

## Context

A Connect-to-server dialog is conventionally drawn with a user-name field, a
password field, and a "remember this password" box. Building those means
deciding where the password goes, and QindaQt has already decided that it
does not decide: `docs/wiki/architecture/secret-service.md` records that
gnome-keyring's secrets component is the session's only Secret Service
provider and that **QindaQt code never reads or writes collection contents**
(ADR-0135). `NetworkLocation::canonicalize()` independently refuses any
address carrying userinfo (ADR-0137), so a user name cannot travel in a
browsed or saved address either.

Meanwhile the platform already answers the question. Every KIO job the file
manager creates carries KIO's standard UI delegate (ADR-0151), so a server
that demands credentials gets the desktop's own prompt — with its own
remember option, resolved by the platform's password service — and a failure
still arrives back as a typed result. For `sftp`, KIO's worker uses libssh
and honours `~/.ssh/config` and the ssh agent, which is how `ssh qinda` and
`ssh qinda-top` already work in both directions without any password at all.

## Decision

**QindaQt's file manager collects no credential and stores no secret.**

- The Connect-to-server dialog has a type, a server, a port, a folder, a
  name, and a sidebar toggle. It has no user-name field, no password field,
  and no credential-source choice, and it says so in plain words: a password,
  if the server needs one, is asked for by the system when connecting.
- A saved location (ADR-0194) holds a name and a canonical, userinfo-free
  address. There is nothing in it to protect.
- Authentication happens where it already happened before this lane: inside
  the KIO job, through the platform's delegate and password service.
- Every seam that reaches KIO — the listing backend, the openers, the
  renamer, the folder creator, the copier, the mover, and the new transfer
  worker — keeps its own independent refusal of embedded userinfo. That
  refusal is defence in depth, not redundancy: each is the last check before
  a different facility.

This is a narrower contract than "remember the password in the Secret
Service" as the wave plan sketched it. It is narrower on purpose: the wider
version would make QindaQt a secret writer, which the accepted architecture
forbids, and would require relaxing an accepted address guard to carry a user
name.

## Consequences

- No new dependency, no keyring client, no wallet, and no code path in
  QindaQt that has ever seen a password.
- SFTP to a host reachable through `~/.ssh/config` works with no prompt,
  which is the configuration both of this user's machines already have.
- An SMB share needing a user name distinct from the local one relies on the
  platform prompt each session, and QindaQt cannot offer to remember it.
  Making that possible means a `network-locations-v2` user-name field and a
  re-examination of the ADR-0137 no-userinfo guard.
- The dialog's own text is the user-facing contract and is asserted by the
  installed-package UI probe, so the promise cannot be dropped silently.

## Revisit when

A server the user needs cannot be reached without QindaQt holding a
credential, or the platform stops providing a credential prompt for KIO jobs.
Either would be a reason to reconsider — and would need
`docs/wiki/architecture/secret-service.md` changed in the same breath.
