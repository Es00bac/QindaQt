# ADR-0125: Host desktop-zone applets on the desktop surface

- **Status:** Accepted
- **Date:** 2026-09-10
- **Owners:** Applet runtime, Shell composition
- **Supersedes:** None
- **Superseded by:** None

## Context

The applet manifest vocabulary has always named a `desktop` placement zone,
but no hosting path existed for it: every resolved applet instance reached
QML through a panel, and profile schema v1 could only declare applets
inside panels. A desktop icon surface — places icons living beside the
wallpaper, a right-click desktop menu, New Folder — therefore had no
declarative home and no resolution path. The QindaQt Bliss experience
([ADR-0124](0124-add-qindaqt-bliss-luna-option-set.md)) needs exactly that,
and a wallpaper-adjacent icon layer is the last obvious stock surface that
profiles cannot express.

The constraints carry over unchanged: resolution must keep the five fail-
closed gates (manifest, placement, host, audited registry, capability
policy); desktop-zone code is a built-in like any other and must not grow
authority by placement; and profile loading stays strict — no new coercion,
no duplicate identity, no lossy round trips
([ADR-0006](0006-profile-global-applet-identity.md)).

## Decision

1. **Profiles declare desktop applets.** Profile schema v1 gains one
   optional root object, `desktop`, whose `applets` array holds the same
   applet objects as a panel. Instance ids share the profile-global
   namespace: an id used on any panel cannot be reused on the desktop, and
   validation rejects duplicates across panels plus desktop as one pool.
   The section round-trips only when non-empty, so profiles without
   desktop applets serialize exactly as before.
2. **One resolution path, no panel edge.**
   `AppletInstanceResolver::resolveDesktopBuiltin` resolves desktop-zone
   instances through the same gates as panel instances, taking the zone
   from the desktop hosting context instead of deriving orientation from a
   panel edge. Failure at any gate produces the same typed statuses and no
   grants; a desktop instance never bypasses the audited registry or the
   capability policy by being off-panel.
3. **One desktop surface per output.** The shell hosts resolved
   desktop-zone instances on a dedicated background-layer desktop surface
   per output (`QindaQt.Shell.DesktopSurface`), rendered between the
   wallpaper and ordinary windows. The surface is shell-owned presentation;
   it grants no file, launcher, or compositor authority by existing.
4. **The desktop-icons applet** (`qindaqt.applets.desktop-icons`) is the
   first desktop-zone built-in: it presents places as desktop icons that
   open in the QindaQt File Manager through the existing
   PlacesController/FileManagerFolderOpener path, with configurable
   placement (`left` or `right`), icon size, and a right-click desktop
   context menu in three styles (`windows`, `mac`, `traditional`). The
   `windows` style may expose an XFCE-style Applications menu behind a
   configurable modifier key (`shift` by default). Its manifest requests
   only the existing `applications.launch` capability.
5. **New Folder is a least-authority seam.** The desktop context menu's
   folder creation goes through a `NewFolderController` that writes only
   under the user's Desktop directory and nothing else. Folder creation
   rides the audited-builtin trust decision — no new capability enum is
   added, and third-party desktop packages gain no write authority from
   this decision.

## Consequences

- Any profile can now carry desktop applets; absence of the `desktop`
  section keeps a profile's persisted form and runtime behavior unchanged.
- The built-in inventory grows to twenty-six manifests and twenty-five
  audited registry entry points, with the desktop-icons dispatcher path
  exercised beside the panel dispatcher rather than inside it.
- Desktop-zone instances consume the same profile-global identity, strict
  settings-value rules, and typed error contract as panel instances;
  loader and resolver tests cover the new section, duplicate rejection,
  and the desktop resolution path.
- The `desktop` zone stays reserved for shell-hosted built-ins: hosting
  remains gated by the audited registry, so the zone cannot become a
  third-party code path without a new trust decision.
- Revisit this decision when a desktop applet needs authority beyond place
  launching and the Desktop-scoped folder seam — for example arbitrary
  filesystem mutation or compositor surface control — or when third-party
  desktop packages become a product goal.
