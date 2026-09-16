# ADR-0172: Applications is a place, and a docked window can replace itself

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** First-party applications (File Manager) with Platform (compositor)
- **Supersedes:** None (extends [ADR-0164](0164-shared-application-catalog-and-file-manager-applications-browser.md)
  and [ADR-0165](0165-workspace-picker-slots-replaced-by-launched-applications.md))
- **Superseded by:** None

## Context

ADR-0164 gave the File Manager an Applications browser over the shared
application catalog, and ADR-0165 made a *reopen-dialog picker* replaceable: the
application its user chooses takes the picker's exact place in the container.

Both were reported as not working, in the plain sense that the feature a user
looks for was not reachable and did not behave as described:

- **Applications was only in the Go menu** (`go.applications`, Ctrl+Shift+A).
  The described expectation is the macOS one: a thing in the sidebar that says
  *Applications* and shows the installed applications.
- **Replacement happened only for a picker.** A File Manager docked in a
  container that launched an application from Applications spawned it as an
  ordinary new window; nothing took the File Manager's place. ADR-0165's
  mechanism was bound to windows the compositor had itself registered as
  placeholders when reopening a saved layout.

## Decision

### Applications is a place

`PlacesController` publishes an `applications` place beside Home, File System,
Trash and Network. It carries an **empty path** for the same reason Network
does — there is no navigable directory behind it — and `PlacesSidebar` routes
both through a shared `routePlace` predicate: neither is emphasized by path
comparison, navigated to, nor accepted as a drop target. Activating it raises
the same `go.applications` action the Go menu uses, so there is exactly one
route into the browser.

Its icon is a new `folder-applications` place glyph: a folder carrying two
rounded app tiles and a dot. Three large marks stay legible at the 16 px a
sidebar actually draws, where four smaller ones turn to mud; the symbolic cut
uses two offset tiles and no third mark, because any two-above-one arrangement
reads as a face in monochrome.

### A docked window may replace itself

`ChooseApplicationForActivePicker` gains a second accepted case. When the active
window is **not** a registered picker, the request is still accepted if that
window belongs to the caller and is a member of a container: the compositor
registers a replacement for it on the spot and proceeds through ADR-0165's
existing launch, correlation and atomic `ReplaceMemberWindow` path. Nothing new
is invented for the swap itself.

The gate is the caller's identity. `KWinControlEndpoint` gains `QDBusContext`
and resolves the **bus daemon's** credential for the calling connection, which
is compared against KWin's authenticated client PID for the active window. A
caller can therefore only ever replace its own window. Without that check the
route would let any client on the session bus replace whatever window happened
to be focused — someone else's application — and that is precisely why ADR-0165
originally restricted it to compositor-created placeholders.

The File Manager tries this route **first** on every activation and falls back
to a plain detached launch when it is rejected. A rejection is the ordinary
undocked case, not an error, so it is never surfaced. Trying the compositor
first also means terminal-required and D-Bus-activatable entries work while
docked, because the compositor owns the full desktop-entry launch facility that
the local fallback deliberately does not.

## Consequences

- Applications is visible where users look for it, and launching from it while
  docked replaces the File Manager in place, which is the described behavior.
- A File Manager run outside a QindaQt session, or before the compositor is up,
  behaves exactly as before.
- The compositor now answers one production route with caller-credential
  checking. That machinery is available to any future self-replacement.
- Every activation makes one short blocking D-Bus call before falling back. The
  compositor answers a rejection immediately, and an absent service fails at
  interface construction.

## Revisit when

- A gesture exists to open a **new split** already showing Applications. That is
  the remaining half of the reported outcome: it needs a user gesture (pointer
  and keyboard), a pending-arrival correlation for the launched File Manager,
  and a topology split against the active member. The self-replacement above is
  what makes such a split useful once it lands, because the placeholder it
  creates can already replace itself.
- More than one window of a process can be active-adjacent; the PID comparison
  would then need a per-window credential rather than a per-process one.
