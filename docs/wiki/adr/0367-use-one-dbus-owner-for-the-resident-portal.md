# ADR-0367: Use one D-Bus owner for the resident portal

- **Status:** Proposed pending installed fresh-login lifecycle qualification
- **Date:** 2026-10-08
- **Owners:** Session supervisor and resident portal
- **Supersedes:** The supervisor-spawned child lifetime clause of [ADR-0318](0318-native-portal-foundation.md); keeps [ADR-0054](0054-export-appearance-through-the-standard-settings-portal.md) activation.

## Context

At the R23 laptop login, the standard xdg-desktop-portal frontend requested
`org.freedesktop.impl.portal.desktop.qindaqt` through its installed D-Bus
descriptor and systemd unit. QindaQt's session supervisor independently
started the same executable as a child. The session child owned the backend
and Portal1 names; the systemd copy failed native composition, retried and
hit its start limit. Resetting that stale unit failure did not remove the
race. The active session portal remained usable, but ordinary service
monitoring reported a failure.

The upstream backend authoring contract requires a D-Bus-activatable service.
Removing the descriptor or trusting a fixed delay would trade one race for
startup failure. [XDG Desktop Portal backend integration](https://flatpak.github.io/xdg-desktop-portal/docs/writing-a-new-backend.html)
and the [D-Bus activation specification](https://dbus.freedesktop.org/doc/dbus-specification.html)
define that boundary.

## Decision

The installed D-Bus descriptor/systemd unit is the sole process-launch
authority. After the shell and selected display are ready, the session
supervisor requests that standard name asynchronously on a dedicated session
connection. It never spawns a competing portal child. Its existing bounded
unique-owner/UID check then attaches the selected display through Portal1;
missing backend or failed attachment cannot block login.

The resident retains the first selected unique caller, even if its display
attachment fails. It watches that exact caller for disappearance and exits
cleanly on loss, so a later login starts a fresh process and first-caller
lease. A compositor/display interruption only revokes current authority;
it does not hand the lease to a different caller. Bus loss still exits.
The separate protected capture broker is neither activated nor launched by
this change.

## Consequences and checks

No new package, service name, wire method or host configuration is introduced.
A short private-bus test covers first-caller loss and replacement refusal;
the existing supervisor owner-replacement test covers selected connection
retention. Focused compilation and tests must pass before source acceptance.
A fresh installed login must show one backend owner, Portal1 attachment and no
failed duplicate systemd unit before delivery can be called qualified.
The current R23 package does not contain this repair.
