# ADR-0151: Authenticated SMB/SFTP browsing through KIO's standard UI delegate

- **Status:** Accepted
- **Date:** 2026-09-12
- **Owners:** File Manager
- **Supersedes:** ADR-0137's clearing of the KIO job UI delegate, which forced `AuthenticationRequired`
- **Superseded by:** None

## Context

ADR-0137 introduced the `NetworkDirectoryBackend` seam for read-only
asynchronous listing of `smb`/`sftp` locations and deliberately cleared every
`KIO::ListJob`'s UI delegate and delegate extension. The stated reason was
that a suppressed prompt converts into a typed `AuthenticationRequired`
result instead of an unexpected dialog. In practice this makes authenticated
shares permanently unreachable: a kioslave that needs a password or mount can
only ask through the job's UI delegate seams, so every such share failed with
no way for the user to supply a credential — the opposite of the requested
usable network browsing.

The platform already ships the supported facility for this interaction:
`KIO::JobUiDelegate` (KIOWidgets) implements the credential and mount prompt
path, and KIOWidgets registers both the default job UI delegate factory and
the default `JobUiDelegateExtension` when the library is loaded. Building a
QindaQt credential dialog, secret store, or protocol client was explicitly out
of scope.

## Decision

`KioNetworkDirectoryBackend` no longer clears the UI delegate seams on the
`KIO::ListJob` objects it starts for supported `smb`/`sftp` locations.
Supported jobs keep the platform's standard KIO UI delegate, so a slave that
needs credentials or a mount shows KIO's ordinary prompt and the user can
browse the authenticated folder; cancellation or a failed prompt still maps to
the typed `AuthenticationRequired` result through the unchanged error table.

- The support library links `KF6::KIOWidgets` privately so the delegate
  factory and extension registrations actually load in the process; KIO types
  stay out of every public header.
- QindaQt installs no custom delegate and gains no credential authority: URL
  userinfo remains refused by `NetworkLocation::canonicalize()`, and QindaQt
  never reads, logs, stores, serializes, or owns a credential.
- `NavigationController` cancels the superseded pending generation before
  every replacement remote request (refresh, remote-to-remote navigation,
  guest clearance), so a job's prompt can never outlive the navigation that
  replaced it.
- Unsupported or malformed schemes still fail before job creation, the
  entry bound, generation/URL fencing, typed error mapping, and local-only
  mutation guards are unchanged, and destruction/cancellation still kills
  pending jobs quietly, taking any open prompt down with its job.

## Consequences

- Opening an authenticated `smb://` or `sftp://` folder now offers the
  platform's normal KIO authentication flow instead of an immediate typed
  failure.
- The File Manager support library's private KIO dependency grows from
  `KF6::KIOCore` to `KF6::KIOCore` + `KF6::KIOWidgets`; KIOWidgets pulls Qt
  Widgets into the link closure of the app and its tests.
- `qindaqt.file-manager-kio-network-backend` now proves, through the existing
  job-creation test seam, that a supported job retains both delegate seams;
  the row still creates no network request (the job is never started).
- Remote regular-file opening and remote mutations remain separate, explicit
  later outcomes; this ADR grants listing no new mutation or launch
  authority.

## Revisit when

Remote file opening/mutation needs a download/cache policy (a new ADR), or
evidence shows KIO's standard delegate is unsuitable in the QindaQt session
(e.g. it requires a widget parent the shell cannot provide) — any replacement
must come through the same independent review, not a local prompt rework.
