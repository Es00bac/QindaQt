# ADR-0260: Show the File Manager's menu when no application is active

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** Global menu, Desktop controls, File Manager
- **Supersedes:** None
- **Superseded by:** None
- **Extends:** [ADR-0033](0033-canonical-menu-model-and-authenticated-menu-ownership.md),
  [ADR-0056](0056-adopt-standard-appmenu-dbusmenu-transports.md),
  [ADR-0077](0077-acknowledge-global-menu-hosting-before-hiding-local-menus.md),
  [ADR-0130](0130-window-attached-menus-without-a-global-menu.md)

## Context

The global menu only ever showed the authenticated active window's own
menu. With no application window active the facade was empty and the bar
collapsed to nothing, which wasted the most visible place on screen exactly
when the user is at the desktop. On macOS that state shows Finder's menu:
Finder *is* the desktop, so its File, Edit, View, Go, Window, and Help menus,
plus the system items, are always one click away.

QindaQt already has every action such a menu needs, each behind an existing
shell controller: the system menu controller and its session-actions facade
(ADR-0070), the places inventory opened through the launcher's bounded process
seam (ADR-0062), the launcher, the clipboard applet, the workspace controller,
the gather overview's single door, the Settings route launcher, the desktop
shortcut note (ADR-0084), and the desktop-icons surface (ADR-0125). The
compositor already reports the desktop surface, docks, and shell popups as
"no active window" (only ordinary application windows are admitted).

Alternatives considered:

- *Make the desktop menu a registrar provider.* It would need a bus name and
  a PID-authenticated proof it cannot honestly have, and its invocations would
  cross the dbusmenu path meant for applications.
- *Put a fallback provider inside the transport coordinator.* Every change
  there risks the authenticated application path (ADR-0033/0077).
- *Arbitrate in QML.* Presentation must never decide which authority an
  activation reaches.

## Decision

1. **A second, shell-owned facade channel.** `GlobalMenuAppletAccess` keeps
   its application channel (written only by the transport coordinator) and
   gains a desktop channel (written only by the desktop menu controller). The
   presented state is the application channel whenever it has anything to
   show (a tree, or a retained inert projection during a provider
   transition); otherwise the desktop tree; otherwise the application
   channel's unavailable or degraded truth. The desktop menu therefore fills
   only the empty state and never competes with an application's menu. Every
   activation is routed by the presented channel: desktop ids leave through
   `desktopActivationRequested` and can never reach dbusmenu `Event`;
   application ids can never run a shell command. One counter mints every
   projection generation, so a delegate rendered from one channel never
   matches the other. The coordinator reads only application-channel facts
   (`applicationAvailable`, `applicationProjectionRetained`), so application
   ownership, authentication, invocation guarding, and the hosting
   acknowledgment are unchanged.
2. **Selection follows the compositor-authenticated identity.** The shell's
   one exact-owner window-actions client gives a presence: an available
   identity without an active window (nothing, the desktop surface, or a
   shell surface focused) shows the desktop menu, actionable. An active
   application window turns a shown desktop menu inert at once and the
   application's first tree replaces it the moment it lands; a menu-less
   application's retained inert menu is withdrawn after the same 500 ms the
   coordinator uses for its own presentation grace. An unavailable or
   rereading identity keeps a shown menu actionable (its own popup appearing
   invalidates the identity) for at most that grace.
3. **It exists only with a hosted global menu.** The desktop menu is enabled
   exactly while the adopted layout hosts a granted global menu (Ready or
   Degraded). ADR-0130 layouts keep menus in windows and show no desktop menu.
4. **The menu is the File Manager's.** The first menu is titled like the File
   Manager ("File Manager", its desktop entry's GenericName) and carries the
   system items; then File, Edit, View, Go, Window, and Help:
   - *File Manager:* About This Computer (Settings → About this computer),
     System Settings…, Keyboard Shortcuts… (Settings → Input → Shortcuts),
     Lock Screen, Log Out…, Suspend, Restart…, Shut Down….
   - *File:* New File Manager Window (the File Manager's desktop entry through
     the launcher, so the window honours File Manager's own start-folder
     preference), New Folder (on the Desktop), Find… (the launcher's search).
   - *Edit:* Paste (onto the Desktop), Select All (desktop icons), Show
     Clipboard History.
   - *View:* Show Desktop, Gather Overview, Clean Up (desktop icons).
   - *Go:* Home Folder, Desktop, Documents, Downloads, Music, Pictures,
     Videos, Computer (the places inventory, opened in the File Manager).
   - *Window:* the workspaces, as one radio group (switch).
   - *Help:* QindaQt Help (the Welcome application), Keyboard Shortcuts (the
     desktop shortcut note, checked while shown).
   Each action appears once. Entries whose owner is absent in this session or
   layout are omitted (and a menu left empty is omitted); entries whose owner
   exists but refuses right now (a session action it does not admit, a
   pending request, nothing to paste) are disabled. The canonical model has
   no per-item reason field, so the reason is the owner's own feedback text
   and the controller's `lastFailure()`, never a label.
5. **One File Manager vocabulary.** File Manager's menu titles, action labels,
   descriptions, and shortcuts move into a small public catalog,
   `src/apps/file_manager/public/file_manager_menu_catalog.*` (target
   `qindaqt_file_manager_menu_catalog`, values only, `public/` exported).
   File Manager's AppShell action catalog and the desktop menu both project
   it; neither spells a shared string. Desktop entries backed by a File
   Manager action use that action's id and label (`file.new-folder`,
   `edit.paste`, `edit.select-all`, `go.home`, and `file.new-window`, a File
   Manager command the window menu does not offer yet).
6. **No unhonoured shortcuts.** The desktop surface is a focus-less layer
   (`KeyboardInteractivityNone`), so none of the File Manager window
   shortcuts fires there. The desktop menu shows no shortcut text rather than
   a key that does nothing. The catalog still carries every shortcut; the day
   the desktop context honours one, the menu shows the catalog's own
   sequence and nothing else.
7. **Routing only through existing boundaries.** Every command reaches the
   one controller that already owns it, through its public boundary: the
   system menu controller (and the session-actions facade it lends, exactly
   as its QML does), the places controller, the launcher (`activate`,
   `requestOpen`), the clipboard applet (`requestOpen`, added here on the
   launcher's precedent), the workspace controller (whose own revision fence
   refuses a switch from a stale menu), the Settings route launcher, the
   gather overview's `toggle`, the shortcut note's `toggle`, and the
   desktop-icons surfaces through a command channel whose requests run the
   surface's own right-click path (New Folder, Paste, and Clean Up on the
   primary output's surface only; Select All on every surface). No command
   carries a program, path, or URL; no D-Bus trust is added. Find… and Show
   Clipboard History are offered only while the adopted layout hosts a ready
   launcher or clipboard applet that answers the request.
8. **Session-ending items confirm first.** Log Out…, Restart…, and Shut
   Down… ask the system menu's own questions through a provider confirmation
   on the facade: one token per question, answered once by the one renderer
   that claims it, declined by any other close, and re-checked against the
   owner's admission when the answer arrives.
9. **The indicator names the File Manager.** While no window is focused and
   the desktop menu is presented, the active-application applet shows the
   File Manager's title and icon (from the facade's desktop-menu facts, read
   only under the applet's existing `windows.read` grant) and offers no
   window actions.

## Consequences

- The bar is never empty at the desktop in a hosting layout, and the desktop
  and a File Manager window read as one program. Recent, Network, and Go to
  Folder… are not offered: File Manager has no Recents place yet and no
  launch route to its Network hub or location prompt. They arrive with the
  File Manager boundaries that provide them (the Finder integration program,
  W11/W12), not through a new process contract here.
- The bold application title is the active-application applet's (DemiBold);
  the global menu keeps one font for every entry because its measured
  overflow geometry binds a single set of font metrics. In layouts without
  the indicator (macOS-inspired) the first menu names the application.
- The command HUD and palette's menu-action search see the desktop menu while
  it is presented, and activate it through the same facade routing.
- Tests: the facade's desktop channel and confirmations; the pure builder
  against the canonical bounds, the File Manager catalog, hostile workspace
  names, and absent or refused capabilities; selection, hand-back, grace, and
  activation against the real facade; routing of every command to its real
  controller over the desktop-controls doubles; provider selection end to end
  with the production global-menu composition on a private bus; every stock
  profile resolved through the panel dispatcher's path; keyboard traversal,
  activation, and confirmation through the compiled renderer offscreen; the
  active-application title; and File Manager's unchanged catalog.
- Live verification on an installed session (desktop click, application
  hand-back, each item) remains a manual check; the rows above are offscreen
  or private-bus proofs.

## Revisit when

Revisit when the desktop surface becomes keyboard-interactive (then show the
catalog shortcuts it honours), when File Manager gains Recents or launch
routes to Network and Go to Folder (then add those entries through them), or
if an application toolkit starts exporting an application-named first menu
that should replace the File Manager title convention.
