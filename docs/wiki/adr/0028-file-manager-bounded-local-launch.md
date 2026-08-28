# ADR-0028: Open File Manager files through a bounded local launch intent

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** First-party applications / File Manager
- **Supersedes:** None
- **Superseded by:** None

## Context

File Manager S0 needs to open a regular file the user selects. A full desktop
"open with" experience requires a MIME-type database, a per-type default/
alternative handler list, and eventually a portal-mediated chooser so a
sandboxed caller can hand a file descriptor to an out-of-process handler
without owning arbitrary execution authority itself. None of that exists yet
in this repository, and S0's acceptance is a local-navigation slice, not a
complete "open with" experience.

The owning behavior and limits are in
[QindaQt File Manager](../apps/file-manager.md). Text Editor's own local,
optimistic, atomic-persistence contract in
[ADR-0022](0022-keep-text-documents-local-and-atomic.md) already established
that first-party apps stay local-only and defer shared infrastructure until a
second app needs it; this record covers the analogous choice for *opening*
rather than *persisting* a file, plus records that the second-app evidence
ADR-0022 anticipated now exists but has not yet justified extraction.

## Decision

File Manager S0 validates a selected entry synchronously (exists, resolves any
symlink once to its canonical target, is a regular file, is readable) and then
hands the canonical local path to `QDesktopServices::openUrl()`. This delegates
handler selection entirely to the desktop's existing MIME/default-application
configuration; File Manager owns no MIME database, handler list, or launched
process lifetime. A validation failure or a false return from `openUrl()`
surfaces as a typed `LaunchError` and a bounded, dismissible presentation
banner; it never blocks navigation or crashes the window.

File Manager S0 also does not import the in-flight `QindaQt.AppShell 1.0`
module (not on this outcome's public base and not yet independently reviewed
or integrated). It composes its own minimal `QQmlApplicationEngine` window
directly against public `QindaQt.Tokens 1.0` and `QindaQt.Controls 1.0`,
following the same precedent Text Editor S1 set for local persistence.

## Consequences

- Opening a file never grants File Manager a general command-execution
  surface: it never constructs a shell command line or interprets
  metacharacters, and it only ever launches a path it has itself confirmed is
  a readable regular file.
- A dangling symlink, a dangling target discovered mid-resolution, a directory
  masquerading as a file at listing time, and an unreadable target are all
  rejected before any handler dispatch is attempted.
- `QDesktopServices::openUrl()` returning `true` only means dispatch was
  accepted; File Manager cannot observe or report the launched application's
  own success or failure. This is a documented boundary, not a defect to
  paper over with a fabricated confirmation.
- Custom handler selection, a "open with" chooser, portal-mediated launch for
  a sandboxed caller, and MIME-type-aware icon/action presentation remain
  later outcomes layered on top of this same validated-path contract.
- File Manager and Text Editor are now two independent first-party apps each
  composing their own QST/Controls-or-Widgets presentation directly. This is
  the second-app evidence ADR-0022 named, but neither app owns a shared
  window/action boundary yet; `QindaQt.AppShell 1.0` is a separate in-flight
  candidate outcome, and adopting it here is deliberately deferred to a later
  File Manager slice rather than folded into this record.

## Revisit when

A shell-wide "open with" chooser, sandboxed portal-mediated file launch, or an
accepted, integrated `QindaQt.AppShell 1.0` public boundary exists. Any of
those would justify replacing this local `DesktopFileLauncher` (or File
Manager's bespoke window composition) with the shared contract instead of
widening this ADR's scope invisibly.
