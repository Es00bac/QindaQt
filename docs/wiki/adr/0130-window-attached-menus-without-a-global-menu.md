# ADR-0130: Attach menus to windows when the layout has no global menu

- **Status:** Accepted
- **Date:** 2026-09-11
- **Owners:** Shell global menu, Qt platform theme
- **Supersedes:** None (narrows the unconditional registrar residency of the
  G2 composition described with
  [ADR-0056](0056-adopt-standard-appmenu-dbusmenu-transports.md), and the
  menubar delegation of
  [ADR-0115](0115-share-appearance-through-qt-platform-theme.md))
- **Superseded by:** None

## Context

The production shell started `com.canonical.AppMenu.Registrar` at every
startup, whatever the layout. That well-known name is a session-wide signal:
Qt's generic platform theme hides each new `QMenuBar` behind a D-Bus menubar
while the name has an owner, and it caches its first answer for the whole
process. Eight of the eleven stock layouts (GNOME-, MATE-, NeXTSTEP-, and
XFCE-inspired, minimal, QindaQt Bliss, Windows classic, Windows modern) host
no global-menu applet, so ordinary Qt applications lost their in-window menus
while nothing displayed them. First-party AppShell applications kept theirs
only because they wait for a hosting acknowledgment
([ADR-0077](0077-acknowledge-global-menu-hosting-before-hiding-local-menus.md)).
Layouts are also adopted live
([ADR-0122](0122-adopt-saved-layout-preferences-live.md)), so a decision made
once at startup is wrong after the user switches layout.

Qt does not export `QDBusMenuBar` for direct construction, and its generic
theme exposes no way to reset the cached registrar answer.

## Decision

1. **Registrar residency follows the resolved layout.** The shell owns the
   registrar name only while the active layout resolves at least one ready
   `global-menu` instance through the same `AppletInstanceResolver` path the
   panel dispatcher and desktop surface use, so an instance rejected by
   placement, host, implementation, or policy claims nothing. Startup and every
   live layout adoption apply the same idempotent transition: a hosting layout
   starts residency unless it is already ready; any other layout stops it,
   releases the name, and publishes `unavailable` with reason
   `global-menu-not-hosted`. Re-adopting a hosting layout does not churn the
   owner.
2. **The QindaQt Qt platform theme decides per menubar creation.** It returns
   Qt's D-Bus menubar only when a live bus-daemon query finds an owner for the
   registrar name at the moment a `QMenuBar` is created, and `nullptr`
   otherwise, so the `QMenuBar` stays in its window. The generic theme is
   consulted only after a positive live query, which initializes its
   process-wide cache as available; a registrar that disappears between the
   two synchronous checks leaves that cache negative, which fails safe to
   in-window menus for that process.
3. **First-party AppShell export is unchanged.** Owner loss already restores
   the local menu, and a returning hosting owner hides it again only after its
   exact acknowledgment.

## Consequences

- Layouts without a global-menu applet show application menus inside windows
  for both first-party and ordinary Qt applications; the QindaQt, macOS-, and
  Unity-inspired layouts keep the global menu.
- A `QMenuBar` keeps the mode it was created with until the application
  recreates it or restarts. After a live switch away from a global-menu layout,
  already-open Qt Widgets applications without AppShell export keep a hidden
  menubar until restarted, and after a switch to one they keep their in-window
  menubar. AppShell applications follow the switch live.
- Each `QMenuBar` creation in an application using the QindaQt theme costs one
  synchronous `NameHasOwner` round trip to the bus daemon.
- Desktop controls that read the global-menu facade (command palette and HUD
  menu actions) find it unavailable in layouts without the applet; the stock
  `minimal` layout's command palette therefore lists no menu actions.
- Evidence: `GlobalMenuRuntimeCompositionTest::layoutHostingFollowsResolvedGlobalMenuInstances`
  and `::registrarResidencyFollowsLayoutAdoption`,
  `ApplicationMenuExportTest::localMenuReturnsOnRegistrarLossAndHidesForReturningHost`,
  `PlatformServicesTest::menuBarIsDecidedPerCreationFromTheHostProbe`, and
  `qindaqt.qt-platform-theme-menubar-private-bus` against Qt's real generic
  theme. The architecture is described in
  [Global application menu](../shell/global-menu.md#registrar-residency-and-window-attached-menus).

## Revisit when

- Qt exports a supported way to create a D-Bus menubar or reset its cached
  registrar answer, or an existing `QMenuBar` can switch modes, so open
  windows could follow a layout switch live.
- A layout needs menu actions in a command palette or HUD without also
  hosting a global-menu applet.
