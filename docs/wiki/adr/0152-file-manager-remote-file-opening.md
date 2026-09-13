# ADR-0152: Remote regular-file opening through KIO's OpenUrlJob

- **Status:** Accepted
- **Date:** 2026-09-12
- **Owners:** File Manager
- **Supersedes:** ADR-0137's "remote regular-file activation reports a
  truthful typed not-supported launchError" deferral of remote file opening
- **Superseded by:** None

## Context

ADR-0137/0151 made authenticated `smb`/`sftp` folders browsable, but
activating a remote regular file still reported "Opening files from a
network location is not supported yet": listing worked while the user's own
files stayed unreachable. The remaining ways to close that gap were a
QindaQt downloader/cache, a hand-rolled protocol client, or an open-with
framework — each a new authority this module must not own.

The platform already ships the supported facility:
`KIO::OpenUrlJob` (KIOGui) opens a URL with the desktop's default handler
and performs any temporary download a remote URL needs, owning the temp
file's lifecycle. Unlike plain `KIO::Job` it does not auto-install a UI
delegate, so the adapter receives KIO's registered standard delegate
(KIOWidgets, already linked per ADR-0151) and ordinary authentication
prompts work through the same flow as network listing.

## Decision

File Manager gains a narrow `RemoteFileOpener` seam
(`network/remote_file_opener.h`) that `NavigationController` calls for a
remote regular-file activation, mirroring how it already uses
`NetworkDirectoryBackend` for listings:

- `KioRemoteFileOpener` is the production adapter: it re-validates the
  smb/sftp scheme itself (defense in depth, mirroring the listing backend),
  starts one `KIO::OpenUrlJob` per open parented to itself, and maps the
  job result to one `openFinished` signal: empty diagnostic on success, a
  bounded message on typed failure or cancellation. `NavigationController`
  surfaces that as its existing `launchError` text; success retires a stale
  error.
- The opener is injected (default null): without it, remote regular-file
  activation keeps reporting the truthful "not supported yet" error, and
  every existing composition/test is unaffected. The stock application
  composes `KioRemoteFileOpener` in `main.cpp`.
- Each open job is parented to the opener, so the opener's destruction
  (e.g. its `NavigationController` dying) kills every pending job — and any
  prompt it owns — quietly, matching ADR-0151's lifetime contract.

## Consequences

- The support library's private KIO dependency grows by `KF6::KIOGui`
  (`KIO::OpenUrlJob`); KIOCore/KIOWidgets from ADR-0151 remain.
- The standard delegate prompts with QWidgets. Its Open With dialog appears
  when the file's type has no associated application, exactly the prompt a
  user needs to choose one; it is the platform's dialog, not a QindaQt
  picker. QWidget construction aborts a bare `QGuiApplication`, so the File
  Manager process composes a QWidget-capable `QApplication` through the
  shared factory in `runtime/file_manager_application.h` (the UI itself
  stays Qt Quick per ADR-0116), and focused tests compose the same factory
  so they exercise that prompt path under the production application class.
- Remote regular files now open through the desktop's default handler, with
  any needed download handled by KIO; QindaQt owns no downloader, temp-file
  policy, handler picker, or credential authority, and never reads, stores,
  or accepts a credential (userinfo refusal upstream is unchanged).
- `qindaqt.file-manager-navigation-controller-network` proves the injected
  wiring (activation reaches the opener; typed failure/success/error-clear
  behavior; lifetime), and `qindaqt.file-manager-kio-remote-opener` proves
  the production adapter's scheme boundary and retained KIO UI delegate
  with never-started jobs, plus drives one real open of an unassociated
  local file type to KIO's standard Open With prompt under the production
  application class (no application is started; the dialog is dismissed
  hermetically). Neither row touches a network or credential.
- Remote mutation (rename/trash/write on remote folders) remains an
  explicit later outcome and gains no authority from this slice.

## Revisit when

Remote mutation needs its own ADR, or KIO::OpenUrlJob proves unsuitable in
the QindaQt session (e.g. its temp-download policy conflicts with a future
contained-window sandbox); any replacement goes through the same injected
seam and independent review, not a local downloader rewrite.
