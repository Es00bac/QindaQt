# ADR-0296: Own a native Secret Service daemon and session boundary

- Status: Accepted
- Date: 2026-09-29
- Owners: QindaQt program manager and keyring module
- Supersedes: ADR-0135 provider, lifecycle and prompt choices for native deployments
- Scope: PK2 daemon source; PAM, full Settings, portal, import and deployment remain separate

## Context

PK1 provides authenticated bounded storage without transport or process policy.
The owner requires ordinary libsecret clients to use a native provider, while
the work must not install or activate it on the live user bus. The program
plan is `docs/plans/2026-09-28-plasma-free-qindaqt.md`.
A user manager outlives logout, QindaQt does not activate graphical-session.target,
and an activated daemon can exist before the session supervisor starts.
Retaining unlocked secrets across those ownership boundaries is unacceptable.

## Decision

Compose small repository, wire/session, properties/items, prompt-provider and
peer-control collaborators around the public PK1 API. Use the standard Secret
Service protocol, including plain and actual DH/HKDF/AES-CBC exchange, through
OpenSSL EVP. Unique-owner sessions and prompts add isolation to same-UID bus
admission; ordinary unlocked collections remain shared by same-user clients.
Loaded locked search integrity remains explicitly unverified until authenticated
unlock; discovery never permits disclosure.

One no-follow private directory writer lock arbitrates all encrypted files and
the public catalog, independently of bus ownership. A versioned bounded catalog
stores only declared collection labels, IDs, dates and aliases. Those values
are leaked and unauthenticated; no encrypted item metadata or secrets are
duplicated there. The catalog is the visibility commit: create saves encrypted
bytes before publishing catalog/alias; delete removes catalog/alias before
unlinking encrypted bytes. Startup removes undeclared encrypted orphans under
the same lock, rejects missing declared files, and can adopt only native PK1
files when the catalog is absent. Publication failures terminate the broker;
uncertain storage durability locks/reloads state and is never acknowledged as
persisted success. Catalog/alias tampering cannot bypass password authentication.
No file rollback protection is introduced.

Serialize and rate-limit KDF admission; bound callers, sessions, prompts,
items, attributes, wire-secret/password lengths, helper output and socket frames.
Disable core dumps and ptrace-style dumpability before handling credentials.
Use locked zeroing pages for owned application secrets and passwords.
Scrub owned wire/controller copies, while admitting that Qt/QML/D-Bus/QProcess,
provider internals and remote clients may keep independent ordinary copies.
No secret logs/errors/argv/env/tempfiles are allowed.

Use a same-euid SO_PEERCRED Unix socket for bounded unlock/authenticated-rekey/lock.
A single systemd-activated listening descriptor must match the exact private
owned endpoint. No caller-supplied UID is trusted. Later PAM integrates through
this public process boundary and never storage internals.

The supervisor owns an optional installed native child and a dedicated unique
bus connection. Keyring1.AttachSession accepts one such owner; its disconnect
terminates an already activated daemon as well as a direct child. Logout sends
Shutdown only after accepted attachment and releases the connection. Bus/name
authority loss exits and wipes. Source user units declare PartOf graphical-session.target;
activation files and direct supervisor ownership supply actual startup.
When native selection is available, only actual gnome-keyring-daemon Exec
entries are superseded by autostart, without changing the polkit rule.
Live distribution activation-file/PAM transitions belong to packaging/deployment,
not this fixture-tested source change.

Extract the common keyboard-exclusive Wayland role into a small public
AuthenticationOverlay module. Initialize LayerShellQt before QGuiApplication
and configure before native window creation/show, failing closed otherwise.
Keep requester/prompt policy outside that module. A minimal QindaTK helper
requires explicit nonempty creation confirmation, uses an approved pipe for
password transfer, clears fields, and cancels on missing/unsupported platform.
A future PK4 factory may extend presentation without changing storage.

## Consequences

- Ordinary secret-tool/libsecret and SecretStorage clients can execute real
  encrypted secret round-trips against an isolated native daemon.
- There are two public metadata leaks: storage's searchable dictionary index
  and catalog collection/alias information. Neither is encryption-key retention.
- Cross-file recovery is visibility-consistent and sole-writer; acknowledgement
  requires the actual durable save, not merely a staged item or alias.
- The legacy standard encrypted transport uses DH1024 and unauthenticated CBC;
  compatibility does not imply authenticated wire messages or a stronger crypto
  design than the protocol. At-rest integrity remains AES256-GCM.
- Same-user unlocked collection access remains intentional. Core suppression
  does not defend against root, compromised clients or memory snapshots.
- Best-effort cursor output placement remains explicitly bounded pending native
  multi-output Wayland qualification.
- PK3 implements PAM borrowed-token lifetime, login unlock and password-stack
  rekey against the existing socket. Full Settings/portal/import follow later.
- No install, live secrets name claim, real wallet read or user-unit operation
  is part of PK2 acceptance.

The current contracts and executable gates are on
[native keyring daemon](../architecture/keyring-daemon.md),
[storage](../architecture/keyring-storage.md) and
[authentication overlay](../architecture/authentication-overlay.md).

The supervisor decomposition review moves unchanged read-only diagnostics to a separate implementation file. New keyring lifetime policy remains a dedicated collaborator; existing orchestration stays below the hard source-size limit.

[ADR-0300](0300-trusted-native-keyring-pam.md) defines the later PAM token-delivery authority: the existing same-UID control boundary is unchanged, but automatic login tokens require the exact protected system-owned daemon invocation.
