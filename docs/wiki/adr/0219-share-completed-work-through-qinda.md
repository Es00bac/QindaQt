# ADR-0219: Share completed work through qinda

- **Status:** Accepted
- **Date:** 2026-09-19
- **Owners:** Development and native packaging

## Context

The operator develops on qinda and qinda-top and requested a single command to
publish or receive completed work. qinda stays online and already hosts bare
project Git repositories, the QindaGentoo overlay, and a Portage binary server.

## Decision

Reuse those services through QindaGentoo's `tools/qinda-sync`, installed as
`/usr/local/bin/qinda-sync` on both machines. Updates are explicitly invoked.

- `qinda-sync code [checkout]` exchanges the current committed branch through
  qinda. Dirty trees and divergent histories require owner resolution; there
  are no automatic commits, resets, force pushes, or edits to other worktrees.
- `qinda-sync publish ARCHIVE` checks the committed overlay Manifest before
  copying a source archive to qinda's distfile directory. Existing different
  bytes require a package revision, rather than replacing a published archive.
- `qinda-sync` reads exact atoms from the overlay's committed
  `metadata/qinda-delivery`, obtains missing archives and pinned shared Git
  sources, then installs through Portage. Existing different recipes or hashes
  stop synchronization. Existing overlay entries and user settings survive.
- Portage continues to own dependencies, signature checks, binary selection,
  compiler settings, concurrency, installation, and package integrity. The
  helper does not update world or restart the active desktop.

## Consequences

An offline qinda-top catches up when the operator runs the command again.
Completed source, exact package recipes, and their archives must all be
published; an agent's working tree or an uncommitted recipe is not a release.
New project repositories must first exist on the hub. Git conflicts remain
visible rather than being resolved by a file-copy policy. The implementation
belongs to QindaGentoo; the [release procedure](../development/releases.md)
records the cross-repository handoff.
