# Privacy, persistence, and recovery

The desktop handles application content and system actions through different
trust boundaries. The design favors least authority, bounded data, explicit
ownership, and conservative recovery after uncertainty. These are engineering
contracts, not a claim that the project has completed a security audit.

## Authority and loss of trust

Notification presentation requires authenticated unlocked state tied to the
supervised compositor. Unknown state hides content and denies actions.
Descriptor provisioning keeps presentation credentials out of ordinary argv and
environment transport. Owner-bound service clients discard stale responses after
replacement. Global menu, task, and tray adapters validate ownership before
letting presentation invoke another application's action.

Applet capabilities separate observation from activation. Platform-specific
libraries remain inside adapters. Network credential entry is confined to a
separate agent instead of passing secrets through Network1 snapshots. Normal
production sessions do not acquire the harness's synthetic-input authority.
Each exact authentication rule is defined in its focused architecture/protocol
page in the [documentation catalog](catalog/reading.md).

## Passwords and keys

Native deployments use QindaQt's own Secret Service provider, prompt and PAM
bridge. **Settings → Passwords & Keys** exposes collection management through
that service; password entry belongs to the native authentication prompt.
The former GNOME provider was removed from both audited hosts on October 3.
See the [native daemon](../architecture/keyring-daemon.md),
[native PAM bridge](../architecture/keyring-pam.md) and
[Passwords & Keys](../apps/keyring-settings.md).

Automatic login unlock is an intended packaged workflow, but physical fresh-login
acceptance remains open. The October incidents also exposed lost session
attachment and credential-prompt readiness gaps; source acceptance and installed
app authentication are tracked separately in the
[everyday audit](../development/everyday-desktop-audit.md). Do not treat successful
keyring migration as proof that a real application can authenticate.

The historical [GNOME integration](../architecture/secret-service.md) remains
reference for older deployments. It is not an instruction to reinstall its PAM
stack on a native deployment. Applications with their own password databases
continue to own those databases.

## What persists

| Data | Storage or lifecycle contract |
| --- | --- |
| Built-in profiles/themes | Versioned package data; user customization does not overwrite built-ins. |
| User settings | Settings1 schema validation and atomic persistence, with explicit migration/revision semantics. |
| Edited user profiles | Atomic user-profile persistence coordinated separately from confirmed selection. |
| Container model snapshots | Versioned model serialization; complete session/app relaunch is a separate feature. |
| Clipboard content | Volatile bounded history; history policy is default off. |
| Text editor documents | Explicit local UTF-8 file saves with atomic replacement and external-change checks. |
| Editor restore state | Optional paths-only reopening plus a separate owner-private crash-recovery content journal, bounded to 4 MiB per document and 32 journals; explicit Restore/Discard. |
| Terminal profiles | QQ_Term application-local profile/settings storage; PTY/session lifecycle remains application-owned. |
| Display recovery | Injected journal and transaction authority with documented lineage and recovery validation. |
| Color assignment | Confirmed preference intent, distinct from actual compositor color application. |

Use [configuration](catalog/settings.md) for exact key defaults and
constraints, [settings architecture](../architecture/settings-service.md) for
schema/storage behavior, and application/service owner pages for exact paths.
Do not infer persistence from an in-memory model or a visible toggle.

## Failure behavior users can rely on within the declared slice

A pending operation stays pending until authoritative evidence resolves it.
Conflicting revisions require explicit reconciliation. A lost response does
not authorize automatic repetition of a potentially completed mutation. A
file-save failure preserves the editor's dirty text. Invalid profile edits roll
back without exposing a partially valid layout. A replaced service cannot
silently reuse old handles as current authority.

Bounds and recovery are service-specific: consult the exact protocol before
changing timeout values, payload limits, or retry policy. See [quality](quality.md)
for hostile-input and restart tests and [handbook index](index.md) for navigation.
