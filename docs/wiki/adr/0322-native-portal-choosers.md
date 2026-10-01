# ADR-0322: Keep native portal choosers outside the resident

- **Status:** Accepted; private actual frontend/native-input qualification passed
- **Date:** 2026-10-01
- **Amends:** [ADR-0318](0318-native-portal-foundation.md) for chooser families

## Context

FileChooser and AppChooser still route to compatibility providers. Replacing
those rows requires complete standard methods and actual frontend/native-dialog
evidence. The secret-bearing resident must not gain GUI state.

## Decision

Reuse the public RequestRegistry, PortalSessionBinding, ordinary attachment,
native Unlocked admission and foreign-parent adapter. Separate wire policy,
frontend request transport, child lifetime and GUI. A trusted package-relative
helper consumes a newly owned ordinary FD, without ambient display/PATH fallback,
KDE dialog, KIO or command parsing. The original foundation constructor keeps
its symbol and behavior; chooser callers supply the helper explicitly.

The helper owns public Qt widget/file-system model dialogs. File-manager
presentation models are private application implementation; this portal does
not reach into them. A future extraction needs a public shared boundary.
Selection uses literal native paths and returns normalized local file URIs.
It does not create, truncate or reserve files. Single save asks before replacing
an existing regular file; multi-save preserves order and suggests unique names
for collisions. The eventual writer owns the subsequent filesystem race.
Filters and extra choices retain their standard wire values.

AppChooser returns only an offered installed ApplicationCatalog ID and never
launches it. The frontend owns OpenURI launch. UpdateChoices resamples the
injected public catalog and replaces offered candidates. Advertise version 1,
including both ChooseApplication and UpdateChoices; version 2 activation-token
generation awaits a public native token port. FileChooser implements all three
standard methods with version 4.

Bounded private stdin frames carry helper updates. Publication rechecks frontend
owner, token and native authority, then validates the exact offered result.
Close, caller loss through the frontend's Request lifetime, frontend replacement,
parent loss and selected-session loss retire the same child. No restore/replay
contract is added.

## Consequences

Other portal families and zero-scope Inhibit retain their boundaries. Qt Widgets
is an existing project dependency; no new package or resident GUI linkage is
added. Chooser routing selects QindaQt after all advertised methods passed the
private actual frontend/native-input gate. A missing selected attachment fails
with response 2, without retrying a compatibility provider. Those fixtures do
not prove installed/physical-session or sandbox document-permission coverage.

## References

- [Native chooser contract](../reference/portal-choosers.md)
- [Native portal foundation](../architecture/portal-foundation.md)
- [Portal service](../architecture/portal-service.md)
- [Application identity ADR-0303](0303-resolve-window-identity-through-application-catalog.md)
