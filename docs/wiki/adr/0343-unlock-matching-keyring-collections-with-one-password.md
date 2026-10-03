# ADR-0343: Unlock matching keyring collections with one password

- Status: Accepted
- Date: 2026-10-03
- Owners: QindaQt program manager and keyring module
- Scope: Native Secret Service and trusted PAM unlock policy

## Context

The one-time legacy import preserves separate encrypted Secret Service and
KWallet collections. A user who assigned the same password to all of them was
asked for that password once per collection on login. PAM authenticated only
the `login` collection, leaving the imported collections locked. Merging
encrypted items and deleting old collections during login would change stable
collection and item paths, aliases, and storage durability contracts.

## Decision

After a successful authenticated unlock, the daemon may try that same token
against other locked persistent collections. Each store authenticates its own
encrypted snapshot; a failed match remains locked. The existing global KDF
minimum gap paces attempts. The daemon retains a move-only locked-page token
only until this bounded fan-out completes, is superseded, or the session is
lost. Explicit lock cancels pending work. A multi-collection Secret Service
prompt waits for these attempts before requesting another password. The PAM
socket acknowledges the named collection promptly and performs follow-on
attempts in the same daemon, outside PAM's bounded reply deadline.

This changes unlock convenience, not stored passwords, encrypted files,
collection identities, aliases, or item paths. A different password still
requires its own authentication. The Secret Service protocol remains standard;
only the daemon's post-authentication policy changes.

## Consequences

One login password can unlock the user's matching imported collections without
repeated prompts. Separate collections remain visible so saved clients and
legacy provenance keep their paths. A later explicit consolidation requires a
separate migration with conflict, rollback and data-retention evidence. Private
fixtures cover one PAM-style control unlock opening another collection while
preserving its item; wrong-password and rekey tests retain independent store
authentication.

See [native daemon](../architecture/keyring-daemon.md) and
[PAM bridge](../architecture/keyring-pam.md).
