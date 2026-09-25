# Plan: Settings on QindaTK, the Wi-Fi password fix, and a Network applet

- **Date:** 2026-09-23
- **Author:** Claude (Opus 5.5), for Jarrod
- **Status:** Complete (2026-09-25). W0–W20 and W11s are integrated on qinda main (`476de567`) and
  installed on qinda-top as desktop `0.1.0_pre20260924-r3` (QindaTK r5 on both machines). Live checks after
  a fresh login remain. One caveat: the File Manager UI-actions check segfaulted twice under full-suite
  load and did not reproduce in eight focused runs. See the overnight log and docs/HANDOFF.md.
- **Source base:** hub `qinda:~/git/container-wm.git` `main` at `2e415cad` (the r4
  source `d492ee80` plus documentation-only commits). QindaTK hub `main` at `89d5ca3`.
  Overlay hub `QindaGentoo` `master` at `ac4658a`.

## The short version

Twenty-one pieces of work, in the order they should ship:

| # | What you'll notice | Why it matters | Size |
|---|---|---|---|
| **W0** | Joining a **new** password-protected Wi-Fi network brings up the password pop-up again, and the pop-up keeps working after you close it once. | This is a real bug on your laptop today. A fix was made on Sep 22 but installed outside the package, and the r4 update quietly replaced it. | Small (fix already written) |
| **W1** | **Search in Settings:** press **Ctrl+K**, type "wifi", "battery" or "shortcuts", press Enter. | Settings has 21 pages and no search, and only the first 10 have a Ctrl+number shortcut. The other 11 can only be reached by clicking. | Medium |
| **W2** | **Bars** for Wi-Fi signal strength and battery charge, beside the numbers. | A bar reads at a glance; "73%" has to be read. | Small |
| **W3** | "Nothing here" messages (no displays, no tablets, and so on) look the same everywhere. | Nine pages each style this message differently. | Small |
| **W4** | Keyboard shortcuts in Settings → Input are drawn **as keys** (Ctrl, Shift, K), not written as text. | Shortcuts read as keys are recognised faster than "Meta+Shift+K" in prose. Needs a new toolkit release first. | Small, plus a release |
| **W5** | A **Network applet** in the panel, next to Bluetooth, Power, Voice, Clipboard, Notifications and the clock: see your connection, switch Wi-Fi on/off, pick a network, open Network Settings. | The desktop has never had one. It was never planned, not dropped. Everything it needs underneath already exists. | Large |
| **W6** | Settings → Screen saver → **Preview** shows the screensaver itself, full screen, gone on any key or mouse move. No lock screen, no password. | Today Preview opens the lock screen's greeter (by design, ADR-0226), so you have to type your password to dismiss a preview. | Small–medium |
| **W7** | When the **desktop is selected or no window is active**, the global menu shows a **desktop menu** (like the Finder menu on macOS): system actions, places, and desktop/window commands, instead of going blank. | Today the menu bar collapses to nothing when no app is focused, which wastes the most visible spot on screen. Every action already exists in the shell. | Medium |
| **W8** | Desktop icons start **below the top bar**, not underneath it. | The desktop surface covers the whole screen (by design) but icon layout uses the full screen rectangle with a 6-pixel margin, so the first row hides under the top bar. | Small |
| **W9** | **Applications** in the file manager works like macOS: every app as an icon, A to Z, in the normal icon/list views; double-click opens; right-click for Open, Keep in Dock, Add to Desktop, Get Info. | Today it is a separate screen: a 132-pixel strip of category "folders" above a plain list, launching on a single click, with no selection, icons grid, right-click menu, filter or keyboard navigation. | Medium |
| **W10** | File manager basics, part 1: **Open**, **Open With** (recommended apps, Other Application…, Always Open With), Open in New Window, Duplicate, Make Link, Copy Path, Open Terminal Here, New File from templates, Delete Permanently, Put Back from Trash, Compress / Extract, Add to Sidebar. | The right-click menu on a file offers only Cut, Copy, Rename, Copy To, Move To, Move to Trash, Properties. Open With was explicitly deferred (needs a new ADR widening ADR-0029). | Large |
| **W11** | **File manager views on QindaTK**: Icons (real previews), a customizable **Details** view (choose, reorder and resize columns; sort by any; group by kind/date/size), **Columns** (like the Mac's), **Gallery** (big preview + filmstrip), per-folder view settings, and a **File manager style** setting: Finder (default), Explorer (folder tree, address bar) or Commander (two panes, F-key bar). | The Details view has fixed columns (name, size, kind, modified) and there are only two views. Jarrod wants more views and customization, and QindaTK for the file manager. | Large |
| **W12** | **Finder-style integration**: other apps' "Show in folder" opens the QindaQt file manager (standard `org.freedesktop.FileManager1`); the file manager is always in the dock (like Finder); desktop icons get the same right-click menu, Open With, Get Info and Quick Look as a file manager window; the top-left app name reads "File Manager" when the desktop is active. | The pieces exist separately today but don't behave as one program. | Medium |
| **W13** | **Drag and drop onto the dock and panels**, **app pinning** from anywhere (Keep in Dock / Pin to Dock), and **dock groups**: drop apps, folders or files onto the dock to add them; drag to reorder or off to remove; make a named group (e.g. "Office") holding several apps that opens upward like an accordion when clicked. | Today nothing in any panel accepts a drop; the dock holds only up to 16 app IDs, reordered with up/down buttons; no folders, files or groups. | Large |
| **W14** | **Edit mode that really works**: Edit panels → every applet stops working normally and can be dragged anywhere, within a panel or onto another panel, with a live preview; Done to finish. | An edit mode with drag handles exists in code but is hard to find or doesn't work in practice (e.g. 17 right-clicks to cross the top bar); cross-panel drags are the likely gap. | Medium |
| **W15** | **Settings → Customize becomes a layout preset switcher**: pick a built-in layout (qindaqt, Mac-style, GNOME-style, XP-style…) or one of your own; **Save current layout as preset…**, rename, duplicate, delete; a changed built-in shows "Modified" with **Restore original**. All panel editing happens directly on the panels (W14). | The Settings WYSIWYG panel editor is clumsy (Jarrod: "sucks"); editing belongs on the panels themselves. | Medium |
| **W16** | File manager everyday features: **tabs**, **Quick Look** (Space bar preview), type-to-select, **Recents**. | Everyday features every desktop file manager has (split out of the old W11). | Medium |
| **W17** | **Your current layout becomes the default**: `macos-inspired` (the Mac-style menu bar and dock you use now) is what a new user gets and what the desktop falls back to. | Jarrod: the layout on his laptop "is the default layout that I would like to be the default layout". Today the fallback is `qindaqt`. | Small |
| **W18** | **The logo and the Settings icon**: the four-spoke circle logo is removed everywhere; Settings gets a proper settings icon; the system menu gets a new mark (Jarrod picks from renders). The △/□ maximize/restore swap stays exactly as it is. | The "glyph" button style draws ✕ with a dashed sketch stroke that doesn't read as an X; containers keep windows maximized so △ never shows; the four-spoke circle is both the Settings icon and the system-menu logo. | Small–medium |
| **W19** | **More window decoration styles and options**: about ten new button styles beyond flat / traffic lights / glyph / symbols, a legible ✕ in the glyph set, and more decoration options for both windows and containers. | Jarrod wants many more choices; the ✕ in the glyph set doesn't read as an X. | Medium |
| **W20** | **Familiar desktop experiences**: an XP-like and a Windows 11-like experience with legally distinct decorations; new modern BeOS and modern Windows 3.1 (Program Manager / File Manager) experiences; NeXT refreshed; iconify and roll-up used where they fit; the global menu only where the experience calls for it. | Users should be able to pick an experience very close to what they know. | Medium–large |

W0 goes first because it's a live bug, and because W5's "join a new network" depends on it. W1, W2 and W5
can run in parallel. W3 is filler work for whoever is free. W4 waits for the toolkit release. W6 (added later at Jarrod's request) is independent and can go whenever a worker is free.

## Ground rules for this work

- **Where work happens:** qinda only, per the global source-of-truth rule. Every workstream gets its own
  worktree and branch from the exact base above, with explicit file ownership, per `AGENTS.md`.
- **Review (changed 2026-09-24):** Jarrod waived independent review for this plan ("I trust the agents that
  made it"). A candidate integrates on the implementer's own tests plus the manager's integration gates
  (full build, the affected suites, docs checks). Commit messages record the waiver.
- **Orchestration:** Claude orchestrates from qinda-top over SSH, as Codex did for the Sep 23 wave,
  unless you'd rather Codex does it (see Decisions).
- **Nothing is installed by hand.** Every change reaches your machines only through the overlay and
  Portage. W0 exists as a lesson: a hand-installed fix was silently undone by the next package update.
- **Leave QindaTK's uncommitted changes alone.** Live Codex sessions on qinda are working in QindaTK's
  checkout right now (WP-055, WP-071, WP-072). This plan only uses controls already committed on the
  QindaTK hub.
- **Coordinate with the Sep 23 Settings repair wave.** Its A01 (Voice) and A02 (Startup) branches are
  still open. Only A02 overlaps with this plan, and only in one shared test file
  (`tests/apps/settings_center/CMakeLists.txt`). Additive edits there; whichever lands second rebases.

---

## W0: Finish the Wi-Fi password agent fix

**Problem.** When NetworkManager asks the desktop's password agent (`qindaqt-network-secret-agent`,
ADR-0069) for a Wi-Fi password, the request includes the network's IP-address and route settings. Qt
delivers those in a wrapped form (`QDBusArgument`) that the agent's size-limit check doesn't recognise, so
it rejects the request as unsafe and no password prompt appears. Separately, the agent quits when its last
prompt window closes, so the next request finds no agent.

**What exists.** Uncommitted work from qinda-top, Sep 22 19:35–19:42, preserved unchanged on the hub as
branch `wip/qinda-top-network-secret-agent-20260922` (`05fc66f1`, 6 files, +218/−2):

- `secret_request_policy.cpp`: counts the wrapped IP-address and route arrays (`aa{sv}`, `aau`,
  `a(ayuay)`, `a(ayuayu)`) against the existing limits. Unfamiliar formats are still refused.
- `secret_agent_types.cpp` / `secret_agent_types_p.h`: scrubs unwrapped values after checking.
- `app/main.cpp`: `setQuitOnLastWindowClosed(false)`, so the agent stays running.
- `tst_network_secret_agent_dbus.cpp`: covers empty and populated IPv4 arrays and rejects an oversized one.
- `docs/wiki/architecture/network-secret-agent.md`: documents both behaviours.

**Verified facts.** No commit on `main` has touched the agent since Sep 3, and no other branch fixes this.
The laptop's installed r4 binary lacks the fix: the fix's marker string is absent from
`/usr/bin/qindaqt-network-secret-agent` but present in the Sep 22 build. Your two saved networks store
their passwords in NetworkManager (`psk-flags=0`), so they don't need the agent. Joining a new secured
network, or re-entering a changed password, does.

**Steps.**
1. Worktree on qinda from `2e415cad`; cherry-pick `05fc66f1` (the agent is untouched on `main`, so no
   conflicts are expected). Rewrite the commit message as a proper change.
2. Add the missing regression tests: the agent keeps running and stays registered after the last prompt
   closes, and a second request then prompts again. Also cover the IPv6 record formats, which have no test yet.
3. Build and run `tst_network_secret_agent*` plus the broader network suite. Check that no secret appears
   in logs (existing redaction tests).
4. Independent review, integrate, then ship in the next desktop package (see Release).
5. **Live check on qinda-top after installing through Portage:** confirm the installed binary contains the
   fix. Join a secured network that isn't saved (or temporarily forget one and rejoin) and confirm the
   prompt appears. Cancel a prompt, trigger another, and confirm it appears again. Then delete the stale
   `build/dev/qindaqt-network-secret-agent.before-wifi-fix` backup.

**Docs:** the architecture page change is included. Add a task-list/handoff entry at integration.

---

## W1: Search in Settings (Ctrl+K command palette)

**Why this approach.** QindaTK's `Tk.CommandPalette` (committed, and already in the installed QindaTK r4)
is a finished Ctrl+K palette: type to filter, results grouped by section, arrow keys and Enter. Settings
already has everything it needs underneath: `SettingsNavigationController` exposes the route list, and
`selectRouteDestination(route, destination)` can open a specific sub-page.

**Design.**
- **Route metadata:** add optional `keywords` (e.g. Network → "wifi, wireless, ethernet, vpn, internet")
  and `destinations` (sub-pages with titles and keywords) to `SettingsRoute` / `SettingsRouteRegistry`.
  Keep this data in the registry, not in the pages, because the shell must not reach into a route's QML
  internals (module-boundary rule). Only Input currently accepts a destination, so the first version lists
  Input's five sub-pages (Mouse & touchpad, Pen & tablet, Keyboard, Shortcuts, Touch). Other routes get
  keyword search only, until they adopt destinations.
- **Shell:** a new `settings_center/SettingsCommandPalette.qml` wrapping `Tk.CommandPalette`, with
  `QindaQtTheme` so it wears the session's theme. `Main.qml` (410 lines) gains only the instance, a
  `Ctrl+K` shortcut, and a "Search settings" button in the sidebar and compact header. Entries show their
  Ctrl+digit shortcut where one exists. Unavailable routes stay listed but say why, matching the sidebar.
- **Escape trap (AGENT-GUARD in `Main.qml`):** two enabled shortcuts with the same key cancel each other in
  Qt. The host's Escape shortcut must stand down while the palette is open, the same way it already
  yields to the Bluetooth pairing prompt.
- **ADR:** "Search Settings routes through a QindaTK command palette". It extends ADR-0048's registry
  contract (keywords and destinations) and records QindaTK in the Settings shell, following ADR-0227's
  precedent for the Audio route.

**Tests.** Registry: every one of the 21 routes has keywords, route order and digit shortcuts are
unchanged, and destinations are valid. QML (`QT_FATAL_WARNINGS`): Ctrl+K opens the palette; "wifi" finds
Network; "shortcuts" opens Input → Shortcuts; Escape closes the palette without firing the host Escape;
it works in the compact 420×320 layout and by keyboard only. The route-construction witness (ADR-0250)
still passes.

**Docs:** `apps/settings-center.md` (a search section), the ADR, and `mkdocs.yml`.

---

## W2: Signal and battery bars (Tk.Meter)

- **Network** (`settings/network/qml/NetworkAccessPointSection.qml`): a `Tk.Meter` beside the existing
  "73%" label, from the already-numeric `signalStrength`. Keep the text and its accessible name, so no
  information is carried by colour alone.
- **Power** (`settings/power/qml/PowerSupplySection.qml`): the projection publishes only
  `percentageText`. Add numeric `percentage` and `percentageKnown` roles in
  `power_settings_projection.cpp`, and show the meter only when the level is known. An unknown battery
  level shows no bar, never an empty one (the repo's "unknown is not zero" rule).
- **Colour:** QindaTK's ramps treat high values as bad (load, heat). For signal and battery, high is
  good, so use the neutral accent colour. Tie the battery bar to the existing `warningSeverity` so it only
  changes colour when Power itself raises a warning. (See Decisions.)
- **Tests:** meter present and bound to the value; absent when the percentage is unknown; light, dark and
  high-contrast screenshots.
- **Docs:** `apps/network-settings.md`, `apps/power-settings.md`.

---

## W3: Consistent empty states (Tk.EmptyState)

Replace hand-styled "No …" labels with `Tk.EmptyState` (icon, title, wrapped text), keeping each existing
`objectName` so current tests still find them:

| Page | File | Message |
|---|---|---|
| Audio | `AudioStreamSection.qml:185` | No application streams |
| Audio | `AudioVirtualDeviceSection.qml:74` | No virtual devices |
| Clipboard | `ClipboardPage.qml:99`, `:211` | No clipboard content / history |
| Color | `ColorOutputSection.qml:75` | No displays reported |
| Color | `ColorProfileSection.qml:101` | No profiles to assign |
| Display | `DisplayArrangementCanvas.qml:163` | No enabled displays |
| Input | `InputPointerSection.qml:56` | No pointer devices |
| Input | `InputTabletSection.qml:59` | No pen tablet |
| Login screen | `LoginScreenPage.qml:242` | No login themes |

Not included: "No wallpaper" (a selectable option) and the login-screen "No preview" (an image
placeholder). Text-only first; add a "Rescan" action only where the page already has a refresh action.
Each edit is small and can go to whoever owns that page.

---

## W4: QindaTK r5 release, then shortcuts drawn as keys (Tk.KeyCap)

1. **Toolkit release.** The installed `dev-libs/qindatk-0.1.0-r4` pins QindaTK `69c1637`, which does
   **not** include `KeyCap`. Hub `main` `89d5ca3` does (along with Thumbnail, Waveform, Filmstrip,
   TimeRuler and RangeSlider). Add `qindatk-0.1.0-r5.ebuild` to the QindaGentoo hub pinning `89d5ca3`. Run
   the QindaTK QML suite (121 cases at that commit). Do **not** include the uncommitted NumberField, icon or
   catalog changes; those belong to the live QindaTK sessions.
2. **Use it.** In `settings/input/qml/InputShortcutRow.qml`, show `row.keys` with `Tk.KeyCap` when a
   shortcut is set and not being captured. Keep the "Disabled" and "Press keys…" texts, keep the
   `inputShortcutKeys_<n>` object name, and give the keys an accessible name that spells them out. Check
   how multi-binding shortcuts are separated in `keys` before relying on KeyCap's parser.
3. **Package guard (the PillSwitcher lesson).** Bump `qindaqt-desktop` to depend on
   `>=dev-libs/qindatk-0.1.0-r5`. Test the Input page against the **installed** toolkit, not a source
   tree, or a passing build can still crash on "KeyCap is not a type".

---

## W5: Network applet in the panel

**What it is.** A panel applet, `qindaqt.applets.network`, placed with Audio, Voice, Bluetooth and Power
in the default profile. Its icon shows the current state: wired, Wi-Fi with signal strength,
disconnected, or radio off. Clicking it opens a popup with:

- a **Wi-Fi on/off switch** (and airplane mode where Network1 admits it; ADR-0251);
- the **current connection**, with Disconnect;
- **visible networks**, strongest first, marked saved and/or secured; click to join (saved →
  `connectKnownNetwork`, new → `connectVisibleNetwork`, where the password comes from the secret agent's
  own prompt; the applet never handles passwords, per ADR-0069);
- **Rescan** (`requestScan`) and **Network Settings…**, which opens the Settings Network page;
- clear pending, failed and uncertain states for every action, like Bluetooth's request state. An action
  is never shown as done before Network1 confirms it.

**Why it's feasible now.** The public `NetworkClient` (Network1, ADRs 0045/0052/0055) already offers
scan, connect-known, connect-visible, disconnect and radio switching, with operation outcomes. The Settings
Network page already uses it. No new service or NetworkManager work is needed.

**Shape (copying the Bluetooth applet, about 2,350 lines):**
- `src/shell/network_applet/`: controller, presentation, request state, and QML (`NetworkApplet.qml`,
  row delegates), on the same controls as the other applets so the panel looks consistent.
- `src/shell/runtime/networkappletcomposition.{h,cpp}`: owns the `NetworkClient` for the shell.
- Registry entry in `applet_runtime/src/builtin_applet_registry.cpp`, manifest `data/applets/network.json`,
  install rules in `ShellAppletRuntimeInstall.cmake`, and entries in the layout profiles (`qindaqt.json`
  and the gnome-, macos- and unity-inspired profiles).
- **System Status:** that combined indicator covers sound, Bluetooth and power, and also omits network.
  Add a network item to `desktop_controls/system_status_controller` so both views agree.

**ADR:** "Network panel applet over public Network1". It adds a new shell consumer of Network1, with no
credential authority and no change to process boundaries.

**Tests:** controller tests on a fake Network1 transport (connect, refused, uncertain, owner replacement,
radio refusal, no Wi-Fi device); a QML popup test (keyboard only, compact panel, vertical panel); registry,
manifest and profile tests; the nested-session applet row.

**Live check on qinda-top:** switch Wi-Fi off and on; move between your two saved networks; join a new
secured network (needs W0); unplug the USB network and watch the icon follow.

**Docs:** new `shell/network-applet.md`, plus `shell/applet-runtime.md`, `shell/layout-profiles.md`,
`shell/desktop-controls.md`, the ADR, and `mkdocs.yml`.


---

## W6: Screen saver preview without the lock screen (added 2026-09-23, late)

**Problem (reported by Jarrod).** Settings → Screen saver → Preview brings up the lock screen, so
dismissing a preview means typing your password.

**Cause.** This is the documented design, not a slip. ADR-0226 clause 4 previews every saver that the
locker's wallpaper plugin (`data/lockscreen/studio.qinda.screensaver`) can draw, plus `blank`, through
`kscreenlocker_greet --testing`. That shows the real greeter with its password field. The session isn't
locked, but it looks and behaves like a lock. Only savers the greeter can't draw run as their own program
(`ProcessScreensaverPreview::start`, `Kind::TestingGreeter` vs `Kind::SaverProgram` in
`src/apps/settings/screensaver/screensaver_preview.cpp`).

**Fix.**
- **Preview does exactly what idle does.** When the idle timer fires, the session's launcher (`src/session/desktop_controls/production/screensaver_launcher`) runs the saver program with the catalog's arguments (`--screensaver` plus per-saver flags from `screensaver_catalog.cpp`) while the session stays unlocked, and moving the mouse dismisses it. Preview must run that same program with those same arguments (the `Kind::SaverProgram` path already does, but is only used for some savers). Jarrod's report: with a 1-minute idle timeout the saver appears and a mouse move dismisses it without locking, but Preview shows only the lock screen and never the saver.
- Preview always shows the **saver itself**. Any discovered saver runs its own program (the catalog
  already proves the `--screensaver`/`--all-screens` launch contract), full screen, and exits on input,
  exactly as it does when the session is idle and unlocked. `blank` previews as a full-screen black
  window (owned by the Settings process or a tiny helper) that closes on any key, click or pointer move.
  The greeter is never started from Settings.
- Remove `Kind::TestingGreeter` from the preview boundary, or keep it only as an explicit, separately
  labelled "Preview lock screen" action if the lock-screen rendering is still worth offering (default:
  remove it; the Screen lock section is where lock behaviour belongs).
- The preview must not inhibit or trigger the real idle lock, and must be cancellable with Escape. Any
  preview failure is reported on the page, as it is today.
- **ADR:** a new ADR, "Preview screen savers without the lock screen", superseding ADR-0226 clause 4
  (do not rewrite ADR-0226; mark the clause superseded and link the new ADR). Take the next free ADR
  number at the time of the work (0257 and 0258 are reserved by W1 and W5).
- **Tests:** preview-boundary unit tests (every discovered saver maps to its own program; `blank` maps to
  the black-window preview; the greeter binary is never invoked); a route test that Preview never starts
  `kscreenlocker_greet`; existing screensaver outcome tests still pass (`ctest -L settings`).
- **Docs:** `docs/wiki/apps/screensaver-settings.md` and the ADR.
- **Live check on qinda-top:** Preview for each installed saver and for Blank; a keypress or mouse move
  returns straight to Settings with no password prompt; the session's real idle lock still works afterwards.


---

## W7: A desktop menu in the global menu when no app is active (added 2026-09-24)

**Request (Jarrod).** When the desktop is selected, or no window is active, the global menu should offer
useful system items, as macOS does with the Finder menu. Follow-up request: the file manager should be
heavily integrated, as Finder is on macOS. So this menu is **the File Manager's menu** (Finder's role),
shown whenever no other application is active, with system items in its app menu. Its File / Edit / View /
Go labels and shortcuts match the File Manager's own action catalog exactly, so the desktop and a File
Manager window feel like one program. Actions that need a window open one through the File Manager's
public boundary. The active-application name at the top-left reads "File Manager" in this state.

**Today.** The global menu only ever shows the authenticated active window's own menu. With no active
window (or only the desktop surface), there is no provider, the facade is empty, and the bar collapses to
zero entries (`docs/wiki/shell/global-menu.md`; `src/shell/runtime/globalmenuappletcomposition.cpp`,
`src/shell/global_menu/composition/src/global_menu_transport_coordinator.cpp`).

**What already exists to reuse (no new service work):**
- `desktop_controls/system_menu_controller`: About this computer, System Settings, Lock, Log out,
  Suspend, Restart, Shut down (Session1 actions, ADR-0070).
- The places-menu applet: standard user folders opened in the File Manager through the launcher's
  bounded process seam.
- Settings routes (open any Settings page), the launcher/start menu, Gather overview, workspace
  switching, show desktop, the command palette, and the desktop-icons surface's own right-click menu
  (`desktop_surface`).

**Design.**
- A shell-owned **desktop menu provider** publishes a canonical `MenuTree` (the same bounded model every
  app menu uses) whenever there is no authenticated active window, or the active surface is the desktop
  itself. The moment an app window becomes active, its own menu replaces it; nothing about app-menu
  ownership, authentication or the local/global hand-off (ADR-0033/0056/0077) changes.
- Invocations are routed only to the shell's existing controllers through their public boundaries. The
  provider never runs arbitrary commands and never crosses a D-Bus trust boundary it didn't already have.
- Proposed menus (final wording in the ADR):
  - **QindaQt** (the "app" menu): About This Computer, System Settings…, Keyboard Shortcuts…,
    Lock Screen, Log Out…, Suspend, Restart…, Shut Down…. Destructive items confirm exactly as the
    system menu already does.
  - **File**: New File Manager Window, Open…, Find… (the command palette / launcher search).
  - **Edit**: Paste and Show Clipboard History (clipboard applet), plus Select All when desktop icons
    are shown.
  - **View**: Show Desktop, Gather Overview, and desktop-icon arrangement (Sort / Clean Up) when the
    desktop-icons surface is present.
  - **Go**: Home, Desktop, Documents, Downloads, Pictures, Music, Videos, Recent, Computer, Network (the
    places list), plus Go to Folder….
  - **Window**: Gather, the workspaces (switch, new), Minimize All / Show Desktop.
  - **Help**: QindaQt Help (the wiki/handbook), Keyboard Shortcuts.
  - Items whose capability is absent (no Session1, no places, desktop icons off) are omitted or
    disabled with the reason, never shown as working.
- **ADR:** "A shell-owned desktop menu when no application is active", extending the global-menu ADRs.
  Take the next free number (0257–0259 are used).
- **Layouts without a global menu** (ADR-0130) are unaffected.

**Tests.** Provider selection (no active window → desktop menu; app active → app menu; hand-back on
focus change; desktop surface active → desktop menu); the tree validates under the canonical model's
bounds; every item routes to the right controller (fakes); absent capabilities; keyboard navigation of
the bar; profile/resolution coverage (AGENTS.md shell rule); the existing global-menu suites stay green.

**Docs:** `docs/wiki/shell/global-menu.md`, `desktop-controls.md`, the ADR, `mkdocs.yml`.

**Live check on qinda-top:** click the desktop (or close all windows) and the menu bar shows the desktop
menu; open an app and its own menu replaces it; each item does what it says.


---

## The Finder integration program (W8–W12, added 2026-09-24)

**Request (Jarrod).** Update the file manager: it is missing a lot of basic features (e.g. right-click
Open With); fix desktop icons starting under the top bar; make the Applications section behave like
macOS; and integrate the file manager heavily, the way Finder is integrated in macOS. QindaTK may be used.

**How QindaTK is used here.** New pieces are built with QindaTK where a control fits: `Tk.DataTable` for
the Details view (virtualised, sortable, resizable columns — a Finder list view), `Tk.Thumbnail` for icon
previews, `Tk.Popover` for Open With and Quick Look, `Tk.CommandPalette` for Go to Folder. Per Jarrod, the
File Manager's **views move to QindaTK in W11** (an ADR supersedes ADR-0116 for the view layer); other parts
adopt QindaTK as each slice touches them.

### W8: Desktop icons below the top bar (small; ship first)

- **Cause (verified).** `desktop_surface_controller.cpp` anchors the desktop layer to all four edges with
  `setExclusiveZone(-1)`, so the surface deliberately spans the whole output, behind the panels (right for
  the wallpaper and for clicks). But `DesktopIconPlacement.qml` lays icons out in the full output
  rectangle (`{x: 0, y: 0, width, height}`) with only a 6 px `cellMargin`, so the first row sits under the
  top bar.
- **Fix.** Give the placement a per-output **work area**: the output minus the exclusive zones the
  shell's own panels reserve on that output (the shell creates those panels, so it knows their edges and
  extents; an auto-hiding panel reserves nothing, as today). Auto-arranged icons flow inside it; stored or
  dragged positions are clamped into it; a panel change re-flows affected icons.
- **Tests.** Placement with a top bar, bottom dock, side panel, several outputs, an auto-hiding panel,
  and a stored position under the bar; the existing desktop-surface suites. **Live check:** first icon
  sits below the top bar.

### W9: Applications like macOS

- Applications stays a sidebar place (ADR-0172), but shows **every launchable application as an item in
  the File Manager's normal Icon and Details views**: A to Z, app icons at the current zoom, selection,
  keyboard navigation, type-to-select, the filter bar, and sorting (name, category, recently used).
  Categories become **View ▸ Group by Category**, not the primary navigation (the drill-down strip goes).
- **Double-click / Enter opens**; single click selects (today a single click launches). The docked
  "take its place" behaviour (ADR-0172) and the chooser mode (ADR-0165) are preserved.
- **Right-click:** Open, Open in New Workspace, Keep in Dock, Add to Desktop, Get Info (name, comment,
  command, categories, desktop-entry file), Show Desktop Entry File.
- **Drag** an app to the dock or desktop; **drop files on an app** to open them with it (shares W10's
  Open With path).
- Entries still come only from the shared `application_catalog` (ADR-0164); hidden (`NoDisplay`) entries
  stay hidden. New ADR superseding ADR-0164's drill-down presentation.

### W10: File manager basics, part 1 — the right-click set

- **Open With** (new ADR widening ADR-0029's launch contract): recommended applications for the item's
  MIME type from the desktop's MIME associations (`mimeapps.list` + desktop entries, through the shared
  application catalog; no second MIME authority), **Other Application…** (a chooser over all apps), and
  **Always Open With** / set as default (writes the user's `mimeapps.list`, the same store Settings →
  Default Applications uses — coordinate with that route's owner). Launches go through the existing
  bounded launch seam; no shell, no URL handler guessing.
- **Item menu:** Open, Open With ▸, Open in New Window, Duplicate, Make Link, Copy Path, Compress,
  Extract (for archives), Add to Sidebar, Get Info, Move to Trash, **Delete Permanently** (Shift+Delete,
  always confirmed), and **Put Back** inside Trash.
- **Background menu:** New Folder, **New File ▸** (from `~/Templates` plus an empty file), Paste,
  **Open Terminal Here** (the configured terminal), Select All, Sort By ▸, View ▸, Show Hidden, Get Info
  for the folder.
- **Compress/Extract** uses KDE Frameworks' KArchive (the File Manager already depends on KF6 KIO),
  zip and tar.* only in v1, as cancellable background jobs through the existing transfer/mutation
  machinery. New dependency → ADR, and the ebuild RDEPEND gains `kde-frameworks/karchive:6`.
- The same items appear in the menu bar (File / Edit) with standard shortcuts.

### W11: File manager views on QindaTK (rescoped 2026-09-24)

**Request (Jarrod).** "The details view in the FM needs more views/customizations as well. I still think
QindaTK is good for the FM."

**Today.** Two views (Icon grid, Details list) on stock Qt Quick Controls (ADR-0116); Details has fixed
columns (name, size, kind, modified). `preferences-v1` (ADR-0198) already remembers the default view mode,
sort column/direction, icon size and hidden files globally.

**Design.**
- **The File Manager's views move to QindaTK** (new ADR superseding ADR-0116 for the File Manager's view
  layer; chrome can follow gradually). The views keep consuming the same navigation/entry models, so every
  place — folders, search results, network locations, Trash, and the W9 Applications view — works in every
  view, with selection, rubber-band, drag and drop, context menus, keyboard, filter and zoom unchanged.
- **Views:** **Icons** (`Tk.Thumbnail` tiles with real previews from the existing bounded preview pipeline),
  **Details** (`Tk.DataTable`: virtualised, sortable, resizable and reorderable columns), **Columns** (macOS
  column view: one column per folder level plus a preview column), **Gallery** (a large preview of the
  selection, a `Tk.Filmstrip` strip of the folder, and its key facts). A view switcher in the toolbar
  (`Tk.Segmented`) and View menu entries with the standard shortcuts.
- **Details customization:** a column chooser (right-click the header, or View ▸ Show Columns): Name, Size,
  Kind, Date Modified, Date Created, Date Accessed, Permissions, Owner, Group, Extension, Path (search
  results), Items (for folders), plus Dimensions for images and Duration for audio/video where the preview
  pipeline can supply them cheaply; column order and widths remembered; sort by any column; **Group by**
  none / kind / date modified / size; folders first; relative dates; compact or comfortable rows; show file
  extensions.
- **Per-folder view settings** (view, columns, sort, grouping, icon size) override the global defaults;
  **Use as Defaults** promotes a folder's settings. Stored in a bounded `preferences-v2` (migration from v1).
- **Performance:** folders of 10,000+ entries stay smooth (virtualised views, metadata fetched lazily for
  visible rows only).
- **Tests:** each view drives production `Main.qml` (selection, keyboard, drag, context menu), column
  chooser and persistence round trips, grouping, per-folder overrides and migration, a large-folder smoke
  test, screenshots of all four views that the worker looks at.
- **File manager styles (added 2026-09-24, Jarrod):** "I don't really want the file manager to deviate too
  much, but maybe it should have some customizations so that it can operate more like a Commander-style file
  manager or a Windows Explorer type thing, or a Mac Finder file manager." One setting in Preferences
  (File manager style) that presets existing options plus two small additions:
  - **Finder** (default, today's feel): places sidebar, Icons/Columns views, no extra bars.
  - **Explorer**: a **folder tree** in the sidebar under the places, an address bar with breadcrumbs,
    Details as the default view, a compact command bar (New, Cut/Copy/Paste, Rename, Delete, Sort, View).
  - **Commander**: **two panes side by side**, each with its own location and view; Tab switches panes;
    copy/move default to the other pane; a bottom function-key bar (F3 View, F4 Edit, F5 Copy, F6 Move,
    F7 New Folder, F8 Delete); keyboard-first.
  Code additions are limited to a one-or-two-pane container and a folder-tree sidebar; everything else is
  presets of options the views already have. Desktop experiences can pick a default style (Mac-style →
  Finder, XP/Windows-like → Explorer, Windows 3.1 → tree beside list).
- **Coordination:** W11 owns the views (`EntryGrid`/`EntryList` and their QindaTK successors). W10 owns the
  right-click set (`FileContextMenu`, actions, launch). W9's Applications integration is ported to the new
  views as part of W11.

### W16: File manager everyday features — tabs, Quick Look, type-to-select, Recents

- **Tabs** (Ctrl+T, Ctrl+W, Ctrl+Tab; drag a file onto a tab), each with its own history.
- **Quick Look**: Space previews the selection in a `Tk.Popover` using the existing bounded preview
  pipeline plus `Tk.Thumbnail`; arrow keys move through the folder; Space or Escape closes.
- **Type-to-select** in every view.
- **Recents** place (the desktop's recently-used store).

### W12: Finder-style integration hooks

- Implement **`org.freedesktop.FileManager1`** (`ShowFolders`, `ShowItems`, `ShowItemProperties`) in the
  File Manager, so "Show in folder" from browsers, editors and download managers opens and selects the
  item in a QindaQt window.
- **Always in the dock**: the File Manager is a permanent first dock item in the default layout (like
  Finder), and a **Trash** item at the dock's end that opens Trash and offers Empty Trash.
- **Desktop icons = one more File Manager view**: the desktop's item and background menus use the same
  action catalog as `FileContextMenu` (Open With, Get Info = the File Manager's properties dialog, Quick
  Look, New File, Compress…), through the File Manager's public boundary (`desktop_file_boundary`).
- The **top-left application name** shows "File Manager" whenever the desktop menu (W7) is showing.
- **System menu (Apple menu equivalent):** add the existing `system-menu` applet at the far left of the
  top bar in the default layout, so About / System Settings / Lock / Log Out / Restart / Shut Down are
  always one click away regardless of the active app (the W7 desktop menu still carries them too).

### Later (not in this round)

- **Open and Save dialogs drawn by the File Manager** (a QindaQt `org.freedesktop.impl.portal.FileChooser`
  backend; today `qindaqt-portals.conf` routes FileChooser to kde/gtk/lxqt). Large; its own plan.
- **Devices and volumes** in the sidebar with mount/eject and per-volume Trash (roadmap S4).
- Tags, column view, batch undo.

### Order and staffing

At most two Opus agents at a time (Codex/Luna is unavailable until 2026-09-29), both building now that review
is waived. Order: **W7** and **W9** (building) → **W17** and **W18** (small) → **W19** and **W13** → **W20** and **W14** → **W15** (after W14) → **W11** and **W10** (one per lane; views vs right-click set) → **W16** → **W12**. W8 and W12 both touch
`desktop_surface`; W8 landed first. W7 and W12 both touch the top bar/active-app; W7 lands first.
Estimate from measured throughput (2026-09-23/24): an implementation takes ~30–60 min (~3 h for the
applet-sized W5), a review round ~30 min, usually one or two rounds, so ~1.5–2.5 h per slice end to end.
With two lanes, W7–W12 is roughly **6–9 hours of wall-clock time**. The binding constraint is usage
limits, not calendar time.


---

## W13: Drag and drop onto the dock and panels, and dock groups (added 2026-09-24)

**Request (Jarrod).** "The dock and panels need to support drag and drop to add items to them, like folders
… grouping … an office group and then put all the office apps in it, and it accordions up."

**Today (verified).** No shell panel or applet has a drop target (no `DropArea` anywhere under
`src/shell`). The dock's pinned items are the Settings1 key `panels.launcherPinned`: a list of at most 16
application IDs (`launcher_bounds.h`, ADR-0076), started, unpinned and reordered with up/down actions only.
There are no folder, file or group items.

**Design.**
- **Drop to add.** The dock (quick-launch applet), and any panel hosting it, accepts drops of applications
  (from the launcher, the File Manager's Applications view, the desktop, a `.desktop` file), folders and
  files. The drop inserts at the pointer position with a live insertion gap. Folders open in the File
  Manager; files open with their default application through the existing bounded launch seam (Open With
  arrives with W10).
- **App pinning everywhere (added 2026-09-24, Jarrod):** pin an app to the dock (or any panel with a
  launcher) from wherever you meet it: right-click a running app in the dock/task list → **Keep in Dock**;
  right-click an app in the launcher, start menu, Applications (W9) or on the desktop → **Pin to Dock**;
  or drag it there. Unpin from the same menus. A pinned app stays in the dock when it isn't running and shows
  a running indicator when it is (one icon, not two).
- **Rearrange and remove.** Drag within the dock to reorder; drag an item off the dock (or use Remove from
  Dock) to remove it. The existing up/down actions stay for keyboard users.
- **Groups.** Drop one app onto another (or choose **New Group**) to make a named group ("Office"); drag
  more apps in or out; rename; ungroup. The group's dock icon shows its first few app icons. Clicking it
  **opens upward like an accordion**: a list of its apps (icon + name) that unfolds from the dock, closing
  on a click outside or Escape; keyboard navigable; built with QindaTK (`Tk.Popover`). One level only
  (no groups inside groups).
- **Folder items behave like macOS stacks**: clicking a pinned folder opens the same accordion with the
  folder's items, plus "Open in File Manager".
- **Storage.** A new structured Settings1 value (e.g. `panels.dockItems`: typed items app / folder / file
  / group with bounded names, paths and children) with a one-time migration from `panels.launcherPinned`,
  bounded like every Settings1 value (item count, group size, name length), written with the same
  confirmed-write/readback behaviour as other Settings1 mutations. New ADR superseding the id-list part of
  ADR-0076.
- **Security.** Pinned paths are data, not commands; opening goes only through the File Manager's public
  boundary and the launcher's bounded process seam.
- **Tests.** Model/migration/bounds tests; drop, reorder, drag-off, group create/rename/ungroup and
  accordion open/close through the real panel QML; keyboard access; profile/resolution coverage (AGENTS.md
  shell rule). **Live check:** drag Office apps onto the dock, group them, click the group.


---

## W14: Drag applets to rearrange panels, within and between panels (added 2026-09-24)

**Request (Jarrod).** "Same thing with all applets and stuff on panels of any kind … move the clipboard from
the left end of the end position on the top bar to the far right … I now have to right-click it, like, 17
times and say 'Move right.' I would just like to be able to click and drag … and between different panels."

**Today (verified).** In the live shell, an applet moves only through its right-click customize menu
(`src/shell/qml/AppletCustomizeMenu.qml`): Move to start / center / end, Move left / Move right (one swap
with the zone neighbour each time, `livecustomizationmodel.h` `stepMove`), Move to panel ▸. But the
customization editor domain **already supports drag gestures** (`docs/wiki/shell/customization-editor.md`,
"Gesture rules"): a preview begins, the dragged applet converges toward each hovered target, cross-panel
and cross-zone moves execute atomically with optimistic fencing, release on an invalid target cancels, and
applied layouts persist through the existing profile store. Only the Settings Customize route drives it;
the panels themselves don't.

**Clarified by Jarrod:** "in edit mode, they stop working normally, and I can just drag and drop them."

**Found while scoping (verified in code):** the live shell **already has an edit mode**: "Enter edit mode"
in the panel and desktop right-click menus (`PanelCustomizeMenu.qml`, `DesktopCustomizeMenu.qml`), a
`PanelEditBar.qml`, and `AppletEditHandle.qml`, which in edit mode paints a ☰ handle and turns each applet
into a drag source whose drop targets `PanelContent` resolves (`beginAppletDrag` / `dropApplet` /
`cancelDrag` on `LiveCustomizationController`). Jarrod has been moving applets with 17 right-clicks, so
either edit mode is hard to find or its dragging doesn't work in practice. Cross-panel drags are the likely
weak spot: each panel is a separate layer-shell window, and a pointer drag that starts in one surface
doesn't naturally deliver hover/drop to another.

**Design (edit mode, end to end).**
- **Entering and leaving:** "Edit panels" in every panel's and the desktop's right-click menu, in the W7
  desktop menu (View ▸ Edit Panels), and a keyboard shortcut; the edit bar shows **Done**; Escape or Done
  leaves. Discoverable, one step.
- **In edit mode applets stop working normally:** clicks, popups and applet menus are inert; every applet
  shows its outline and handle; the whole applet (not just the ☰ glyph) is the drag handle.
- **Drag and drop anywhere:** within a zone, across zones, and **onto another panel, including on another
  display**, with the editor's live preview (the others make room) and an insertion marker. If one panel
  surface can't track a drag into another, the worker picks a mechanism (e.g. a real Wayland
  drag-and-drop with a typed payload, or a shell-owned full-output drag overlay while dragging) and records
  it in an ADR.
- **Release** commits through the existing editor transaction (same persistence, undo/redo and escrow
  rules as the menu actions); release off-target cancels; everything snaps back.
- **Outside edit mode nothing changes:** no accidental drags, so no separate lock toggle is needed.
- **First step for the worker:** reproduce what happens today in a nested session or offscreen harness
  (enter edit mode, drag within a panel, drag to another panel), report exactly what's broken, then fix.
- **Coordination with W13.** W14 owns the panel-level drag of whole applets (the panel QML chain:
  `RuntimePanel`, `PanelContent`, `PanelAppletRow`, `AppletChip`). W13 owns drops of apps, files and folders
  *into* the dock applet and its groups. A drag that starts on a dock *item* belongs to W13; a drag that
  starts on the applet's own handle/empty area belongs to W14. The two lanes agree this split before editing.
- **ADR:** only if a durable new contract appears (e.g. the lock toggle's persisted key); otherwise document
  under the customization editor and panel-surfaces pages.
- **Tests.** Drag within a zone, across zones, across panels and across outputs through the real panel QML;
  applets inert in edit mode and normal outside it; Escape/invalid-target cancel; persistence round trip; the
  existing customization editor suites stay green. **Live check:** Edit panels, drag the clipboard from one
  end of the top bar to the other in one motion, drag an applet from the top bar to the dock, Done.


---

## W15: Settings switches layout presets; all editing happens on the panels (added 2026-09-24)

**Request (Jarrod).** "The settings WYSIWYG panel editor sucks. All panel/layout customization happens by
editing the panels directly. The settings should just switch between layout presets, which should allow user
presets to be saved as well."

**Today (verified).** Settings → Customize edits the selected layout profile on a WYSIWYG monitor canvas
(`src/apps/settings/customize/qml/`: canvas, applet palette, outline, properties panes, pointer gestures,
action bar, profile cards). The selection is the Settings1 key `panels.layoutProfile`; the running shell
adopts a newly saved profile live (ADR-0074, ADR-0122). Profile catalogs merge the installed stock profiles
with a writable **user store** last, so a user-saved profile with the same id overrides the stock one
(`docs/wiki/shell/layout-profiles.md`). Nine stock profiles ship (ADR-0223).

**Design.**
- **Settings → Customize (layout part) becomes a preset gallery:** one card per layout, with a small preview
  (reuse `CustomizeProfileCard`), built-in presets first, then **My presets**. Clicking a card switches to it
  (the existing confirmed Settings1 write; the shell adopts it live, as today).
- **Save current layout as preset…** (name it) copies the currently applied layout — including every edit
  made directly on the panels — into the user store as a new preset. **Rename**, **Duplicate** and **Delete**
  for your own presets (delete asks first; deleting the active one switches to the default).
- **Edited built-ins:** editing panels directly while on a built-in layout saves an override under the same
  id (existing precedence). The card shows **Modified** with **Restore original** (removes the override) and
  **Save as new preset**.
- **The WYSIWYG canvas, palette, outline and properties panes are removed** from Settings. Non-layout
  options that live on that page today (e.g. panel auto-hide timing, the customize chord) stay in Settings
  or move to the panel's own edit menus — nothing is lost.
- **Parity first (hard gate):** before removing anything, the worker lists every capability of the Settings
  editor (add/remove applet, applet settings, add/remove panel, panel edge/position, size, visibility and
  auto-hide, zones, per-output placement…) and confirms each is available directly on the panels (edit mode
  from W14, panel and applet right-click menus, `PanelEditBar`). Missing ones are added to the panel
  experience in this workstream (e.g. an **Add applet…** picker in edit mode) before the Settings editor
  goes.
- **ADR:** supersede the Customize-route editor decisions with "Layout presets in Settings; editing on the
  panels", keeping the profile schema and precedence unchanged.
- **Tests.** Switching presets (live adoption), save/rename/duplicate/delete with bounds and name
  validation, Modified/Restore for built-ins, active-preset deletion fallback, the parity checklist as
  tests where feasible, the route-construction witness (ADR-0250) still green, and the full Settings suite.
- **Order:** after W14 (it relies on edit mode working end to end).

 
---

## W17: Make the Mac-style layout the default (added 2026-09-24)

**Request (Jarrod).** The layout on his laptop "is the default layout that I would like to be the default
layout, the one that appears to basically be macOS."

**Verified.** The laptop's Settings1 `panels.layoutProfile` is `macos-inspired`, the installed stock
profile, unmodified (the user store holds no `macos-inspired` override; its only file is an unused old
`qindaqt.json`). Today a new user and every fallback (service unavailable, saved profile deleted or
renamed) get `qindaqt` (`docs/wiki/shell/layout-profiles.md`, "Shell startup selection").

**Change.** `macos-inspired` becomes the default selection in the Settings1 schema and the shell's
fallback, and the default in Settings' layout presets (W15). Other layouts are unchanged and still
available. The theme default is not changed unless Jarrod asks (his theme is `qinda-nocturne`). ADR:
"The Mac-style layout is the default", amending ADR-0223's default and the startup-fallback rule; docs
updated. Tests: schema default, shell fallback with no/unknown/deleted selection, Settings default.

## W18: Window button glyphs and the QindaQt mark (added 2026-09-24)

**Request (Jarrod).** "The PlayStation glyph icons in the window manager [should] be fixed, because the X
doesn't actually look like an X, and the triangle doesn't appear to be used. Try to find some place for the
triangle. And the circle with four spokes … is a bad logo … I want that to go away. I don't want that to be
the settings logo icon either."

**Verified.** Jarrod's `appearance.windowButtonStyle` is `glyph` (the PlayStation-style set:
`src/decoration_painter/src/decoration_buttons.cpp`): Close is two diagonal lines drawn with a dashed
"sketch" pen (`setDashPattern({5.0, 1.6, 3.0, 1.2})`), Minimize a circle, Maximize a triangle only when the
window is not maximized, Restore a square. Containers keep windows maximized, so the triangle almost never
appears. The four-spoke circle is `data/icons/QindaQt/scalable/apps/preferences-system{,-symbolic}.svg` and
`org.qindaqt.Settings{,-symbolic}.svg`, used by Settings (`Icon=preferences-system`) and by the system menu
applet (`SystemMenuApplet.qml`).

**Change.** (Updated: Jarrod loves that △ means Maximize and □ means Restore — **keep that swap as is**. The
✕ legibility fix moves to W19 with the other button styles.)
- **Four-spoke circle removed everywhere.** Settings gets a proper settings icon (e.g. a clear gear or a
  sliders motif) in colour and symbolic forms; the system menu gets a new mark (a few candidates rendered
  for Jarrod to pick). No other icon keeps the
  four-spoke motif.
- **Show before shipping:** the worker renders the old and new button sets and icons at 1x/1.5x/2x in light
  and dark, puts the screenshots in the plan folder, and describes them to Jarrod by voice; the final choice
  is his.
- Tests: decoration painter golden/pixel tests updated; icon coverage tests; docs (iconography, visual
  identity).

---

## Desktop experiences: decorations and layouts (W19–W20, added 2026-09-24)

**Jarrod's constraint for this group:** "Most of this is just theming and modifying a few configuration files.
I don't want you to over-engineer this … implement this making the fewest changes to the existing code as
possible without causing problems elsewhere." So: theme and layout JSON first; code only where a new
look genuinely needs drawing; no new frameworks, services or settings machinery.

**Keep the Mac-style experience as it is (Jarrod):** "the current macOS style desktop … it's excellent … with
the few changes that I've asked you to make on it, that's about that." Only the already-requested items touch
it (W7 desktop menu, W13/W14 drag and drop, W17 default, W18 logo).

**What already exists (verified).** Decoration options in Settings1: window and container decoration,
window and container button style (`flat`, `traffic-lights`, `glyph`, `symbols`), button side, which buttons,
title alignment, container tab order, container button glyph visibility. Nine layouts, each paired with a
theme (ADR-0223); three show the global menu (Menu and Dock, QindaQt, Command Rail) and the rest already put
menus in windows (ADR-0130). The window manager can iconify windows and roll them up (shade).

### W19: More window decoration styles and options

- **About ten new button styles** without ten new painters: generalise the button painter once into a small
  **style spec** (button shape: circle / rounded square / square / pill / tab / none; fill: solid /
  outline / gradient / bevel / glass; glyph family: lines / symbols / letters / dots; size and spacing;
  hover and pressed treatment), then ship the new styles as named specs in data. Candidate set:
  **Aqua** (glossy gel circles), **Classic bevel** (raised 3D squares), **Luna-like** (rounded blue squares
  with a red close; our own shapes and colours), **Fluent-like** (flat wide rectangles, red close on
  hover), **Tab** (BeOS-style yellow tab with a close box), **NeXT** (small square buttons with bold
  glyphs), **Minimal lines** (thin glyphs, no plates), **Pills** (one pill holding all three), **Dots**
  (tiny coloured dots that grow glyphs on hover), **Outline** (thin rings), **Chunky** (large touch-sized
  buttons). Final list and names after renders.
- **✕ legibility:** the glyph set's close button is drawn with a dashed "sketch" pen and doesn't read as an
  X — solid strokes; check ○/□/△ the same way. **Keep △ = Maximize, □ = Restore.**
- **More options for windows and containers** (each a small Settings1 appearance key, applied the same way
  as today's): button size, button spacing, title-bar height, corner radius, title font weight, show the app
  icon in the title bar, a roll-up (shade) button, and what double-clicking the title bar does (maximize /
  roll up / minimize). Containers get the same button-style list as windows.
- Tests: painter pixel/golden tests per style at 1x/2x, light and dark; settings round trips; the existing
  decoration suites. Renders go in the plan folder for Jarrod.

### W20: Familiar desktop experiences

Each experience = one layout profile + one theme + a decoration style + whether the global menu shows.
Mostly JSON, using W19's styles.

- **XP-like** (today "QindaQt Bliss"): must *feel* like XP — blue title bars with rounded tops, green start
  button feel, taskbar with quick launch, menus in windows — but be **legally distinct**: our own colours,
  gradients, glyphs, shapes and wallpaper; no Microsoft names, logos, sounds, fonts or artwork.
- **Windows 11-like** (today "Centered Taskbar"): centred taskbar, rounded corners, flat buttons with a red
  close, menus in windows; legally distinct in the same way. **Decided (Jarrod):** it should feel like
  Windows 11 for people switching — minus the advertising, nagging and upsells: no ads, no promoted apps, no
  web results in search, no account nags; just a clean, good experience.
- **Modern BeOS (new):** yellow tab title bars (W19 Tab style), a Deskbar-style panel in the top-right
  corner, in-window menus, roll-up on double-click.
- **Modern Windows 3.1 (new):** a Program Manager-style desktop (program groups as desktop windows, using
  the dock/desktop groups from W13 where possible), minimized windows iconify onto the desktop, a File
  Manager arrangement with a folder tree beside the file list, in-window menus, classic bevel buttons.
- **Modern NeXT (refresh "Workspace Dock"):** miniwindows (iconify to the dock column), NeXT-style buttons,
  menus per the NeXT feel.
- **Iconify and roll-up** are used where each experience expects them (Win 3.1 and NeXT iconify; BeOS rolls
  up).
- **Global menu** appears only in experiences that call for it (Mac-style, QindaQt, Unity-style);
  everywhere else menus stay in windows (existing ADR-0130 behaviour).
- **Names:** user-visible names avoid Microsoft trademarks and product/artwork names ("Windows", "Luna",
  "Bliss") — e.g. "Classic Blue" / "Modern Taskbar" — while internal ids stay unchanged so saved selections
  keep working.
- Tests: profile/theme validation and resolution tests for each experience; screenshots of each for Jarrod.

---

## Code-first round (W10–W20, decided 2026-09-24)

**Jarrod's process:** write every remaining feature first, with each lane reading the other's work, and don't build
anything until all of it is written; then one combined build and a debug pass.

**Round branch:** `round/code-first-20260924` on qinda, from `main` once W7 and W17/W18 (built and tested the
normal way) are integrated. Each item starts from the round branch's current head; when it's written and
peer-checked, the manager merges it into the round branch. At the end the round branch gets one full build and
all test suites, the authors fix failures in their own areas, and it merges to `main`.

**Lanes (updated 2026-09-24 for speed; Jarrod gave Claude authority over the process):**

| Lane | Voice | Order |
|---|---|---|
| A — panels and dock | Jessica (`claude-helper-one`) | (W17/W18 finishing) → **W13** → **W14** → **W15** |
| B — file manager | Harry (`claude-helper-two`) | (W7 finishing) → **W10** → **W11** → **W11s** → **W16** |
| C — decorations, integration, experiences | Adam (`claude-helper-three`) | **W19** (started now) → **W12** → **W20** |

Items touching the same code stay in one lane. W16 also exposes Quick Look to desktop icons through the File
Manager boundary; W12 reuses W10's actions. **All layout-profile JSON edits live in W20**; the Trash dock item type is
part of W13; W11 is split into W11 (views) and W11s (Finder/Explorer/Commander styles).

**Process:** no builds until every item is written; workers configure once and syntax-check each changed file
(`.cache/claude-plan-20260923/syntax-check.sh`, ≈1.5 s/file); no separate peer-review pass (the per-file compiler
checks plus the final full build and test suites are the gate); each item starts from the round branch head and is
merged into it on handoff. Then one full build, all test suites, fixes by area, release.

**Reserved ADR numbers:** W19 0264, W13 0265, W14 0266, W15 0267, W20 0268, W10 0269, W11 0270, W11s 0271,
W16 0272, W12 0273 (0262 = W9, 0263 = W17).


---

## Release and install

1. **Desktop r5, soon:** W0 alone, so the Wi-Fi fix doesn't wait for the larger work.
2. **QindaTK r5:** W4 step 1, in parallel.
3. **Next desktop releases:** W1, W2, W3, W4, W5, W7 and W8 as each is accepted, then W9–W12 as each is accepted, depending on `qindatk-0.1.0-r5`.
4. For each package: build on qinda, push the overlay change to the QindaGentoo hub, install on qinda-top
   through Portage only, run `qcheck`, restart the affected services, then do a **fresh login** and the live
   checks above. Note: qinda itself is still on desktop r2.

## Decisions I need from you

1. **Ship the Wi-Fi fix on its own first (desktop r5)?** Recommended: yes.
2. **Bar colours for signal and battery:** neutral accent colour, coloured only on a battery warning
   (recommended), or coloured by level?
3. **Who orchestrates:** Claude from this laptop (recommended, with spoken updates) or Codex?
4. **Network applet placement:** default profile only, or all four layout profiles? Recommended: all
   four, plus System Status.
5. **System Monitor:** the same QindaTK r5 release unlocks its improvements (Meter tooltips, Ctrl+K
   palette, inline error notices, a sampling-speed control). Add those as a follow-up after this plan?

## Risks

- **Mixed toolkits in one window.** QindaTK controls sit inside pages built on QindaQt.Controls. The
  `QindaQtTheme` bridge maps the same QST-1 tokens, and the Audio route already does this, but check it
  with light, dark and high-contrast screenshots.
- **Toolkit churn.** QindaTK is under heavy live development. Pin exact commits and never build against
  its working tree.
- **Keyboard conflicts.** Ctrl+K and Escape must be tested against every route's own shortcuts.
- **Open wave branches.** A01 and A02 are unmerged; the plan avoids their files except the one shared
  test file.
- **Scope creep in W5.** Keep version 1 to what `NetworkClient` already offers. VPN, hotspot and wired
  profile editing stay in Settings.

## Not doing

- Moving all 20 remaining Settings routes to QindaTK (a much bigger change, needing its own ADR).
- Replacing the Audio console's own fader, knob and lamp controls. QindaTK still has none of them;
  moving them into QindaTK is the better future step.
- Swapping Appearance's `SegmentedChoiceRow` for `Tk.Segmented`. That only makes sense if Appearance
  moves to QindaTK.
