# ADR-0077: Acknowledge global menu hosting before hiding local menus

- Status: Accepted
- Date: 2026-09-05
- Supersedes: the local-menu visibility decision in [ADR-0068](0068-compose-first-party-menu-export-through-appshell.md)

## Context

First-party applications export their menus through the standard AppMenu
registrar, but successful publication only proves that an endpoint is
available. It does not prove that the selected shell profile contains a live
global-menu renderer, or that the shell authenticated, decoded, and displayed
that endpoint. Hiding an in-window menu from publication or configuration alone
can therefore make every menu action inaccessible. Keeping both menus visible
after the shell really hosts the endpoint creates a duplicate menu.

The standard registrar protocol has no hosted-visibility acknowledgment. The
application and QindaQt shell need a small additive contract without weakening
the existing focus, process-identity, and menu-validation boundaries.

## Decision

The QindaQt registrar adds `IsMenuHosted(service, path)` and
`MenuHostedChanged(service, path, hosted)` on its existing D-Bus object and
interface. A live global-menu renderer holds a lease. The shell acknowledges an
exact endpoint only after it passes the existing focused-window authentication,
dbusmenu decoding, and canonical export steps while at least one lease exists.

AppShell starts with its local menu visible. It hides that menu only after an
acknowledgment from the exact current registrar owner for its exact exported
service and path. Registrar owner loss or replacement, renderer loss, endpoint
withdrawal, or a registration, authentication, client, decode, export, or query
failure withdraws the affected proof and restores the local menu.

Acknowledgments remain in a bounded per-endpoint set across ordinary focus
changes while a renderer lease exists. This keeps inactive application windows
at a stable height and lets their menu remain globally available when focused
again. A failure is checked against the client and endpoint generation that
originated it, so a queued failure from a retired client cannot withdraw a
newer binding. This state is transient process state and is never persisted.

A foreign registrar that implements only the standard AppMenu interface cannot
provide this acknowledgment. First-party applications keep their local menus
visible in that case.

## Consequences

- A first-party menu has one visible host when the QindaQt shell has actually
  accepted it, while a usable in-window fallback remains available otherwise.
- Shell composition lifetime and profile configuration are insufficient proof;
  the renderer lease represents a live presentation host.
- The contract is additive to the registrar interface. Standard exporters and
  foreign registrars continue to interoperate without learning the extension.
- No new invocation authority is introduced. Existing exact-owner, focused
  window, PID, decode, and action-consent checks remain authoritative.
- Tests cover renderer attach/detach, registrar and announced-owner loss,
  endpoint failure, stale-client callbacks, and focus changes that retain a
  valid acknowledgment.

## Alternatives considered

Hiding local menus whenever the global-menu applet is configured was rejected
because service applet compositions can exist in profiles with no rendered
global menu. Treating registration as hosting was rejected because a registrar
may accept an endpoint that no renderer consumes. Clearing every acknowledgment
on focus changes was rejected because it causes avoidable window-height changes
and loses the known capability of inactive endpoints.
