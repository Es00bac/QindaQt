# ADR-0086: Route GlobalShortcuts only to a verified backend

- Status: Accepted
- Date: 2026-09-06

## Context

ADR-0059 fixed a fail-closed portal selector where every reviewed
non-Settings family is explicitly ordered `kde;gtk;lxqt` and an unlisted
family is closed by `default=none`. That uniform order was safe for the
families reviewed at the time because the frontend filters the ordered names
by the providers that actually advertise the requested implementation
interface at runtime; an inert name in the list simply loses the race.

A compatibility investigation for a third-party dictation utility
(`gabbee`) found that its only global-hotkey mechanism is
`org.freedesktop.portal.GlobalShortcuts`, and that this family was entirely
absent from `qindaqt-portals.conf`. Under `default=none` this makes
`CreateSession`/`BindShortcuts` fail closed for every application on QindaQt,
not only Gabbee. Adding GlobalShortcuts needs the same explicit review ADR-0059
requires for any new family.

Unlike the ten already-reviewed families, GlobalShortcuts is not uniformly
available: reading the installed backends' own `.portal` metadata on this
host (`/usr/share/xdg-desktop-portal/portals/{kde,gtk,lxqt}.portal`) shows
`org.freedesktop.impl.portal.GlobalShortcuts` in `kde.portal`'s `Interfaces=`
list, but absent from both `gtk.portal` and `lxqt.portal`
(`xdg-desktop-portal-gtk` 1.15.3 and `xdg-desktop-portal-lxqt` 1.4.0). Global
hotkeys are also more sensitive than the existing fallback families: an
unreviewed or merely aspirational entry would document a consent-bearing
capability no installed non-KDE backend actually implements.

## Decision

`qindaqt-portals.conf` adds exactly one line:
`org.freedesktop.impl.portal.GlobalShortcuts=kde`. GlobalShortcuts is ordered
to `kde` alone, not the uniform `kde;gtk;lxqt` fallback order, because `kde`
is the only installed backend whose own metadata advertises the interface.
QindaQt still implements no part of GlobalShortcuts itself; its `.portal`
file continues to advertise only Settings, matching ADR-0059's authority
boundary.

The paired contract-text boundary test
(`tests/services/portal/check_boundary.cmake`) compares the selector file
byte-for-byte, so any future widening of this one line back to
`kde;gtk;lxqt` (or to any other backend) fails the build until that backend's
`.portal` metadata is re-verified to advertise GlobalShortcuts and the line is
deliberately reviewed again; a matching negative case in
`check_boundary_negative.cmake` proves the checker rejects the uniform
three-way order for this family. The Portal P1 frontend-integration row
(`tests/services/portal/tst_portal_frontend_integration.cpp`) reads the real
installed `kde.portal` metadata at test time and fails loudly if it ever stops
advertising GlobalShortcuts, then proves the frontend actually routes a
`CreateSession` call to an injected fake registered under the `kde` fallback
identity on the private bus, exactly as the existing FileChooser proof does
for the other families.

## Consequences

- Applications that use the standard GlobalShortcuts portal (including
  Gabbee) can bind global hotkeys under QindaQt when a real KDE portal
  backend (`xdg-desktop-portal-kde`) is installed and running, the same way
  FileChooser and the other reviewed families already resolve to it.
- GlobalShortcuts remains unavailable (fail-closed `UnknownInterface`,
  consistent with ADR-0059) on any QindaQt install that lacks
  `xdg-desktop-portal-kde`; this is preferable to listing gtk/lxqt as if they
  implemented a capability they do not.
- QindaQt takes on no consent UI, session, or shortcut-binding
  implementation authority; KDE's existing portal backend and KGlobalAccel
  remain the sole conflict-arbitration and consent authority for shortcuts
  bound this way.
- If a future `xdg-desktop-portal-gtk` or `xdg-desktop-portal-lxqt` release
  adds GlobalShortcuts support, extending the order back to
  `kde;gtk;lxqt` is a deliberate, reviewed edit gated by the same boundary
  test, not a default assumption.

## Revisit when

Revisit if QindaQt implements GlobalShortcuts itself, a non-KDE installed
backend gains verified GlobalShortcuts support, or upstream changes the
GlobalShortcuts interface name or selection semantics.
