# ADR-0097: Separate workspace slots from live windows

- Status: Accepted
- Date: 2026-09-07

## Context

Core can serialize split trees, but compositor window identities expire at
logout. An application ID does not identify the purpose of one of several
terminal or editor windows. Persisting runtime topology alone cannot restore
related work reliably.

## Decision

Add `QindaQt::Workspaces`, depending only on the public Core model and Qt Core.
Its versioned document contains a name, optional container color, layout and
application slots. Layout leaves refer to durable slot IDs. Launch intent uses
desktop-entry IDs and URLs; it never stores shell command strings.

Separate pure assignment policy from filesystem storage and from compositor
adoption. Automatic assignment requires a unique remaining slot and eligible
window for the application. Ambiguity requires an explicit user choice; manual
replacement is supported. Complete assignments produce a fresh Core value;
the compositor remains responsible for atomic adoption and live eligibility.

Use atomic JSON replacement in an explicitly supplied workspace directory.
Reads report damage or unsupported schemas without resetting saved work.

## Consequences

Saved layouts remain meaningful across logout without pretending to restore
an application's unsaved state. Duplicate application windows cannot silently
trade places. This foundation does not itself implement launch, capture,
reopen UI or session adoption; those are required before claiming workspace
restoration works. No new runtime dependency is introduced.

See [Saved workspaces](../architecture/workspaces.md).
