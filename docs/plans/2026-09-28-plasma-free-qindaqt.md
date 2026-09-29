# Plan: QindaQt without Plasma, and QindaQt's own KWin

- **Date:** 2026-09-28
- **Author:** Claude (Opus 5.5), lane PF, for Jarrod
- **Status:** Proposed. This is a plan only: no code, ebuild or profile has changed.
- **Source base:** container-wm hub `main` at `190e49ba`; QindaGentoo checkout at `69e0ce9`
  (`kde-plasma/kwin-6.6.6-r2`, `gui-wm/qindaqt-desktop-0.1.0_pre20260928-r2`, profile
  `qindaqt/systemd`); the KWin `6.6.6` release tarball; the Portage database, journal and running
  session on `qinda-top` on 2026-09-28.
- **Owner's words:** "I want QindaQt to not be tied to any Plasma services; any remaining need their
  own, native, better implementations designed to integrate cleanly in the QindaQt ecosystem." and
  "I want QindaQt's KWin freed from Plasma."

## The short version

Plasma is on the login screen because the package that adds that entry comes with Plasma's
workspace package, and three things QindaQt still uses pull the workspace in: KDE's **lock screen**,
KDE's **power manager** (PowerDevil), and the two **Sloom Studio menu add-ons** for Plasma's panel.
The entry goes away for good only once QindaQt has its own lock screen and power management. After
that, the rest follows: our KWin drops its Plasma parts, and night light, the password prompt,
screenshots, file dialogs, screen sharing and keyboard shortcuts become QindaQt's own.

| Step | What you'll notice | Size |
|---|---|---|
| **M0** (optional, now) | The Plasma entries are hidden from the login screen on the laptop by a Portage setting. Plasma is still installed underneath. | minutes; your call |
| **M1** | **Plasma gone from the login screen for real.** A new QindaQt lock screen: your screensaver behind the password box, touch and on-screen keyboard work, and it stays locked even if its window crashes. QindaQt itself handles lid close, dimming and screen-off when idle, the low-battery action and the brightness keys. KDE's locker, PowerDevil and the Sloom Plasma add-ons are uninstalled. | 16 slices |
| **M2** | **QindaQt's own KWin**: no Plasma activities, Plasma theme libraries, Breeze or Aurorae inside it. Night light is run by QindaQt, per screen. | 8 slices |
| **M3** | One password prompt (QindaQt's), never two racing each other. The Print key opens QindaQt's screenshot tool. No Spectacle crash at logout. | 4 slices |
| **M4** | Open/Save dialogs, "Open with", screen sharing, remote input and app permission prompts are QindaQt's own. KDE's portal (the one that crashes at logout) is removed. | 12 slices |
| **M5** | Keyboard shortcuts are run by QindaQt's own registry inside the compositor; Settings shows every shortcut and who owns it, and catches clashes. | 7 slices |
| **M6** | Portage refuses to bring Plasma pieces back by accident. | 1 slice |

**Total: 48 slices, about 72–120 agent-hours (≈96 at 2 h a slice).** Two lanes can run side by
side (compositor and lock; services), which roughly halves the elapsed time. The lock, lid, suspend
and battery checks need you on the laptop.

## 1. What ties QindaQt to Plasma today

### Why Plasma is in SDDM

```text
plasma.desktop, plasmax11.desktop  <- kde-plasma/plasma-login-sessions (+ kwin-x11 for the X11 entry)
plasma-login-sessions             <- PDEPEND of kde-plasma/plasma-workspace
plasma-workspace                  <- kde-plasma/kscreenlocker  (PDEPEND: the greeter loads Breeze's
                                     LockScreen.qml through plasma-workspace's look-and-feel loader)
                                  <- kde-plasma/powerdevil     (links libkworkspace)
                                  <- kde-plasma/sloom-globalmenu (Plasma panel applet; in the qindaqt
                                     profile's package set and in @world on both machines)
kscreenlocker                     <- kde-plasma/kwin[lock] and gui-wm/qindaqt-desktop
powerdevil                        <- gui-wm/qindaqt-desktop
```

On the laptop nothing else pulls `plasma-workspace` (`qdepends -Q`). On qinda the `@kde-desktop`
set (`plasma-meta`) installs the full Plasma desktop on purpose, so qinda keeps the entry until that
set is dropped (open question 2).

### Inventory

| Package | What QindaQt uses it for | Kind | Without it | Replaced in |
|---|---|---|---|---|
| plasma-login-sessions | Nothing. Ships the two Plasma session entries. | data | nothing | M1 (goes with plasma-workspace) |
| plasma-workspace | Nothing directly, but today's lock screen is Breeze's `LockScreen.qml`, loaded through its look-and-feel package structure; PowerDevil links its libkworkspace. Its own autostarts are `OnlyShowIn=KDE`. | libraries, data, Plasma session services | the greeter falls back or terminates | M1 |
| kscreenlocker | Linked into KWin (`lock` USE): lock state, `org.freedesktop.ScreenSaver` and `org.kde.screensaver` (lock, inhibit, active), auto-lock timer, lock on resume, the `kscreenlocker_greet` process. Settings writes `kscreenlockerrc` (ADR-0091, 0132); the screensaver is drawn by a Plasma wallpaper plugin (ADR-0216). | service inside `kwin_wayland` plus greeter process | no lock screen; apps can't hold the screen on through ScreenSaver; the notification gate (ADR-0011) loses lock truth | PF5–PF8 |
| powerdevil | Supervisor child `org_kde_powerdevil`: idle display-off (ADR-0105), lid and power policy (ADR-0132), per-source power profile, brightness keys and `org.kde.ScreenBrightness` with feedback, critical-battery action, power-management inhibitors. | runtime service | lid close gets logind's default; no idle screen-off; brightness keys dead; **no critical-battery action** | PF1–PF4 |
| sloom-globalmenu, sloom-panelmenu (overlay) | Nothing in QindaQt: Plasma applets only run inside `plasmashell`, and QindaQt has its own global menu (`src/shell/global_menu`). | Plasma applets | nothing in QindaQt | PF9 (open question 3) |
| kwin-x11 | Nothing. Pulled by `plasma-login-sessions[X]`. | Plasma X11 compositor | nothing | M1 |
| kactivitymanagerd | Nothing. D-Bus-activated by KWin's activities support; running in the live session. | service | nothing | PF10 |
| plasma-activities | KWin build option. QindaQt's KWin plugin includes KWin's `activities.h` in two files and finds the package in `src/compositor/CMakeLists.txt`; exact RDEPEND (ADR-0098); `kio-extras[activities]` through the Plasma profile's USE. | library | plugin needs two guards | PF10, PF14 |
| libplasma | KWin's runtime QML (overview, window view, tiles editor, desktop-change OSD, output locator, effect frames, snap outline, on-screen notification, thumbnail-grid switcher). QindaQt's own tabbox uses `PlasmaCore.Dialog`. Also pulled by polkit-kde-agent, kscreenlocker, powerdevil, kirigami-addons and the Sloom applets. | QML runtime library | those QML scenes fail to load | PF11 (M1 and M3 remove the other users) |
| milou | KWin's overview search only. | QML | overview fails to load | PF11 |
| aurorae, breeze | KWin's hard-coded default (`org.kde.breeze`) and fallback (`org.kde.kwin.aurorae`) decoration. Settings lists them only when installed (ADR-0160). | plugins | nothing while QindaQt's decoration loads | PF11 |
| knighttime | Required to build KWin's night light plugin; `knighttimed` computes the schedule (running). Settings writes `knighttimerc` and `kwinrc [NightColor]` (ADR-0136). | service and library | KWin will not configure without a patch | PF12–PF13 |
| kglobalacceld | Embedded in `kwin_wayland` as `org.kde.kglobalaccel`: every KWin, shell, desktop-controls and app shortcut, including "Meta alone". The supervisor also starts `/usr/libexec/kglobalacceld`, but no such process runs in the live session (KWin already owns the name, so it presumably exits). | library inside KWin | every keyboard shortcut | PF22–PF24 |
| xdg-desktop-portal-kde | 16 portal families: FileChooser, AppChooser, Access, Email, Inhibit, Notification, Print, Screenshot, ScreenCast, RemoteDesktop, GlobalShortcuts, InputCapture, Clipboard, Usb, Account, DynamicLauncher. The laptop has no gtk or lxqt portal, so there `kde` is the only real backend behind `kde;gtk;lxqt` (qinda has both, from its LXQt set). Reported to crash at logout. | service | file dialogs, screen sharing, agent input (ADR-0087, 0088) | PF17–PF21 |
| spectacle | The Print key (`desktop-controls` launches it). Pulls kpipewire. Reported to crash at logout. | application | Print does nothing | PF16 |
| polkit-kde-agent | Supervisor child. Also a PDEPEND of `kauth[policykit]`. Pulls libplasma. | service | no privilege prompts | PF15 |
| polkit-gnome (not Plasma) | XDG autostart (`NotShowIn=MATE;KDE`) runs it in QindaQt too, so it **races the supervisor's KDE agent at every login**: polkitd's log shows the KDE agent winning one session and the GNOME agent the next. Pulled by `sys-auth/polkit[gtk]`. | service | — | PF15 |
| libkscreen, kirigami-addons, kpipewire, kde-cli-tools | Only through the packages above, or the Plasma profile's `plasma` USE (`xdg-utils[plasma]`). | libraries, tools | — | M1, M3, PF14 |

**Kept on purpose:** kdecoration (the API QindaQt's decoration implements; KWin requires it),
kwayland (KWin requires it; desktop-controls uses its DPMS protocol) and layer-shell-qt (shell and
screensaver surfaces). Gentoo files them under `kde-plasma` because they release with Plasma, but
they are KWin's own libraries and run nothing. KDE Frameworks (KConfig, the KGlobalAccel client,
KIdleTime, KIO…) are not Plasma. gnome-keyring stays the Secret provider.

**Other live findings.** The SDDM greeter is configured for Wayland (`01gentoo.conf`) but has no
compositor, so every boot logs `HELPER_DISPLAYSERVER_ERROR` and falls back to X11 (optional slice
G1). Nothing about the greeter depends on Plasma.

## 2. QindaQt's KWin

What each Plasma dependency is in KWin 6.6.6's `CMakeLists.txt`, the switch or patch that removes
it, and what QindaQt supplies instead:

| Dependency | How KWin wires it | Switch or patch | QindaQt supplies | Slice |
|---|---|---|---|---|
| plasma-activities, kactivitymanagerd | `find_package` OPTIONAL; `KWIN_BUILD_ACTIVITIES` dependent option | `-DKWIN_BUILD_ACTIVITIES=OFF` | guards in the two plugin files; drop the `find_package` | PF10 |
| kscreenlocker | REQUIRED when `KWIN_BUILD_SCREENLOCKER` | `=OFF` plus patch `qindaqt-session-lock` | Lock1 service and `qindaqt-lock` greeter | PF5–PF8 |
| knighttime | REQUIRED, no option (`src/plugins/nightlight`) | patch `qindaqt-nightlight-option` adds `KWIN_BUILD_NIGHTLIGHT` | QindaQt night light module and schedule | PF12–PF13 |
| kglobalacceld | REQUIRED when `KWIN_BUILD_GLOBALSHORTCUTS`; embedded in `GlobalShortcutsManager` | `=OFF`, last | registry in QindaQt's KWin plugin with `org.kde.kglobalaccel` compatibility | PF22–PF24 |
| libplasma | RUNTIME only (`ecm_find_qmlmodule` warns, never fails) | ebuild RDEPEND under `plasma` USE | effects that import it disabled; QindaQt QML through `kwinrc [Outline] QmlPath` and `[OnScreenNotification] QmlPath`; tabbox without `PlasmaCore` | PF11 |
| milou | RUNTIME, overview only | same | overview disabled; QindaQt's gather overview already owns that role | PF11 |
| breeze, aurorae | RUNTIME under `KWIN_BUILD_DECORATIONS` | keep the option ON; drop the RDEPEND; patch `qindaqt-default-decoration` makes QindaQt's decoration the default and fallback | — | PF11 |
| KWin KCMs (kcmutils, knewstuff, kdeclarative, kxmlgui) | `KWIN_BUILD_KCMS` | `=OFF` | QindaQt Settings already replaces them | PF10 |
| KRunner integration | `KWIN_BUILD_RUNNERS` | `=OFF` | nothing uses it | PF10 |

Traps a future agent must know:

- `KWIN_BUILD_DECORATIONS=OFF` also flips the default window placement from Centered to Maximizing
  (`src/kwin.kcfg`). Keep it ON and only drop the Breeze and Aurorae packages.
- With `KWIN_BUILD_GLOBALSHORTCUTS=OFF`, `GlobalShortcutsManager::processKey` stops dispatching
  keyboard shortcuts, KWin's own included. The flag flips only in the same revision that loads the
  replacement registry.
- `--no-lockscreen` and `--no-kactivities` exist only when KWin is built with those features. The
  `qindaqt-wm` launcher already probes `kwin_wayland --help` (`kwincommandbuilder.cpp`); keep that,
  and never pass those flags from an SDDM greeter command.
- Disable the overview, window view, tiles editor, desktop-change OSD and mouse-click effects in the
  session defaults before libplasma and milou leave, or their shortcuts load broken QML. The effect
  frame path (`frames/plasma`) is hard-coded but only mouse-click uses it.
- KWin 6.6.6 has no `ext-session-lock-v1` (searched the tree); the native lock needs the patch.
- Patches stay one series in `compositor/patches/series.json` (checked by `verify-kwin-source`,
  mirrored into the overlay). After M2 it holds five: the decoration cutout, tablet proximity,
  session lock, night-light option and default decoration. Keep them as commits on a
  `qindaqt/v6.6.6` branch of a KWin mirror on qinda and export with `git format-patch`, so a KWin
  upgrade is a `git rebase`, not hand-merged patches.

## 3. Native replacements

### 3.1 Power and idle, replacing PowerDevil (PF1–PF4)

- **One authority.** The resident `Power1` service becomes the power-policy authority (protocol v2).
  It already owns UPower, power profiles, logind and backlight (ADR-0060), so PowerDevil's second
  daemon and its private `powerdevilrc` go away.
- **Lid.** `Power1` takes logind's `handle-lid-switch` block inhibitor only while its policy is
  loaded, so a crash hands the lid straight back to logind's safe default. Actions: suspend,
  hibernate, lock, screen off or nothing, per AC and battery, with a docked rule (an external
  display is connected → nothing).
- **Critical battery.** At UPower `WarningLevel=Action`, a cancellable countdown notification, then
  the chosen action (hibernate, suspend or power off).
- **Profiles.** A power profile per source (AC, battery, low battery) through the existing holds.
- **Inhibitors.** One registry: `org.freedesktop.PowerManagement.Inhibit` compatibility, the portal's
  Inhibit, and ScreenSaver inhibits forwarded from Lock1. Each is tied to its caller's lifetime and
  shown in the Power applet ("Firefox is keeping the screen on"), with an override.
- **Idle engine.** A new `src/session/idle_policy` module in `qindaqt-desktop-controls` (already the
  Wayland client that runs the screensaver launcher). It uses `ext-idle-notify-v1` stages: screensaver,
  dim, display off (`org_kde_kwin_dpms`), lock (Lock1), suspend (Power1). Stages are suppressed while
  Power1 reports an idle inhibition; Wayland surface inhibitors are honored by the protocol itself.
- **Brightness.** The brightness keys call Power1 `SetInternalBrightness` (ADR-0148) and Display1
  `SetOutputBrightness` (ADR-0150); the keyboard backlight goes through Power1 and UPower; feedback
  uses the existing notifier.
- **Preferences** move to Settings1 (`power.lid.*`, `power.idle.*`, `power.critical.*`,
  `power.profile.*`), imported once from `powerdevilrc`. The three `powerdevil_*` adapters and the
  supervisor's PowerDevil child are deleted.
- **Tests.** Fake logind, UPower and power-profiles buses (the PB-2 pattern); invariant tests for
  the lid-inhibitor lifetime and the critical action; unit tests for the idle-stage state machine.

### 3.2 Lock screen, replacing KScreenLocker (PF5–PF8)

- **Compositor (KWin patch `qindaqt-session-lock`).** Implements the standard `ext-session-lock-v1`
  protocol when KWin is built without KScreenLocker. The KSldApp calls behind the existing
  `#if KWIN_BUILD_SCREENLOCKER` sites (11 files) go to a small internal lock state, so KWin's own
  enforcement stays in charge: `isScreenLocked()`, `Window::isLockScreen()`, the input filters,
  effects, screen edges and input method. The patch is estimated at about a thousand lines. Protocol
  rules: `locked` is sent only after every output has shown a lock surface (KWin's
  `LockScreenPresentationWatcher` logic); if the locker dies, the session stays locked on a solid
  fallback and a restarted locker can take over; only QindaQt's locker may bind the global (KWin's
  restricted-interface mechanism).
- **Greeter (`qindaqt-lock`).** QindaTK and QML, one lock surface per output through a small Qt
  shell-integration plugin for session-lock surfaces, modeled on LayerShellQt. It shows the clock,
  the user, the password field, the keyboard layout and the on-screen keyboard (KWin's input method
  is allowed on lock surfaces), and it works with touch. It draws the chosen screensaver's QML item
  behind the prompt, using the same Qinda.Patrol and CircuitReef modules; the Plasma wallpaper
  plugin `studio.qinda.screensaver` goes away. The PAM service `qindaqt-lock` is installed by the
  ebuild.
- **Service (`Lock1`).** `org.qindaqt.Lock1` plus `org.freedesktop.ScreenSaver` compatibility (Lock,
  GetActive, GetActiveTime, SetActive, Inhibit and UnInhibit forwarded to Power1, ActiveChanged). It
  handles logind's Lock and Unlock and takes a `PrepareForSleep` delay inhibitor, so the lock is on
  screen before suspend. It also sets `LockedHint`, keeps the grace period and restarts the greeter.
  `session_lock_state` (the ADR-0011 notification gate) and `session_actions` keep working through
  the compatibility name, then move to Lock1.
- **Settings.** The Screen lock and Screen saver routes move from `kscreenlockerrc` to Settings1
  `lock.*`, imported once.
- **Tests.** A nested KWin matrix: keyboard, pointer and touch never reach a normal client while
  locked; a locker crash keeps the lock; hotplug while locked; unlock only after PAM success;
  lock-before-sleep ordering; the on-screen keyboard on a lock surface.

### 3.3 Night light, replacing KNightTime and KWin's plugin (PF12–PF13)

- **Compositor.** A KWin patch adds `KWIN_BUILD_NIGHTLIGHT` so the upstream plugin, and KNightTime,
  can be left out. A QindaQt KWin plugin module applies colour temperature per output through
  `Output::setChannelFactors` (what upstream uses), with smooth transitions. It adds a per-output
  opt-out, for example a calibrated external monitor (it composes with the `display_color_*`
  modules), and a temporary inhibit for colour picking and screenshots. Live state goes on
  `org.qindaqt.Compositor1`.
- **Schedule.** The existing `night_light` service becomes the schedule authority: sunrise and
  sunset from a manual location or GeoClue2, or fixed times, plus the transition length. It pushes
  targets to the compositor. The Settings Display route moves to Settings1
  `display.nightLight.*`, imported once from `kwinrc [NightColor]` and `knighttimerc`. This
  supersedes ADR-0136.

### 3.4 Polkit agent (PF15)

- `qindaqt-polkit-agent` is built on polkit-qt6 (`sys-auth/polkit-qt`, not Plasma). Its QindaTK
  dialog offers the admin identity choice, the password, retry and cancel, and the action's details
  in plain words, with keyboard, touch and screen-reader support.
- The supervisor starts it in place of the KDE candidates. The autostart runner (ADR-0247) skips
  other agents (polkit-gnome, polkit-kde, lxqt-policykit, mate-polkit), so exactly one registers.
- Profile changes: `sys-auth/polkit -gtk -kde`, and `kde-frameworks/kauth -policykit`, whose
  PDEPEND pulls polkit-kde-agent and so libplasma. KWin's only KAuth use is the helper that kills a
  hung window owned by root (open question 4).

### 3.5 Screenshot (PF16)

- `qindaqt-screenshot` captures through KWin's own `org.kde.KWin.ScreenShot2`, which the shell
  already uses for gather previews; its desktop file declares the restricted interface. It takes
  the full screen, the active window, a picked window or a region (a layer-shell overlay over a
  frozen frame, usable from the keyboard), with a delay option.
- Results can be copied through Clipboard1 or saved to `~/Pictures/Screenshots`. A notification
  offers Open, Copy and Show in folder.
- The Print key in desktop-controls launches it, replacing ADR-0100's Spectacle rule. The same
  capture core serves the Screenshot portal (PF19). Screen recording stays with OBS through the
  desktop's OBS bridge.

### 3.6 Portal backends, replacing xdg-desktop-portal-kde (PF17–PF21)

- **Growth path.** Grow `xdg-desktop-portal-qindaqt`, which today serves only Settings, one family
  at a time. Each `qindaqt-portals.conf` row flips from `kde;gtk;lxqt` to `qindaqt` once its backend
  and its frontend-integration test land; ADR-0133's `default=none` rule stays. The KDE backend is
  the fallback until PF21 removes it.
- **Foundation.** Request and session objects; parent windows through xdg-foreign handles; consent
  dialogs in a separate UI process, so the resident backend never draws; restore tokens in one store
  that Settings can list and revoke.
- **Families, in order:**
  - Access, Notification (to the notification host), Inhibit (to the Power1 registry) and Email.
  - FileChooser and AppChooser: a QindaTK dialog on the file manager's own models (views, places,
    previews, default apps).
  - Screenshot and ScreenCast, over KWin's `zkde_screencast_unstable_v1` and PipeWire.
  - RemoteDesktop, Clipboard and InputCapture, over KWin's EIS. This ends ADR-0088's
    `XDG_CURRENT_DESKTOP=KDE` workaround and keeps ADR-0087's agent input.
  - Print, Account, DynamicLauncher and Usb.
  - GlobalShortcuts, over the kglobalaccel API until M5 and over Shortcuts1 after it.
- Background and Wallpaper stay closed, and Secret stays with gnome-keyring.

### 3.7 Global shortcuts, replacing kglobalacceld (PF22–PF24)

- **In the compositor.** Key interception has to be synchronous with input, so the registry lives
  in `kwin_wayland`, as a QindaQt KWin plugin module with its own input filter at the
  global-shortcut position. It handles press, release and repeat, modifier-only shortcuts ("Meta
  alone") and layout-independent matching, and respects the lock screen and the
  keyboard-shortcuts-inhibitor protocol.
- **Compatibility contract.** It owns `org.kde.kglobalaccel` (the `org.kde.KGlobalAccel` and
  `org.kde.kglobalaccel.Component` interfaces), so KWin's own actions and every KF6GlobalAccel
  client (shell, desktop-controls, KDE apps) keep working unchanged. `kglobalshortcutsrc` is
  imported once.
- **Native interface.** `org.qindaqt.Shortcuts1` serves Settings → Input (one list, the owner of
  each key, clashes shown before saving) and the GlobalShortcuts portal. KWin then builds with
  `-DKWIN_BUILD_GLOBALSHORTCUTS=OFF`, and the supervisor drops its kglobalacceld child.
- **Structure.** The registry policy (pure code), its store and the KWin adapter are separate
  modules.

## 4. Slices and order

| # | Outcome | Main paths | Needs | Slices |
|---|---|---|---|---|
| PF0 | *Optional, not counted.* Hide the Plasma entries on the laptop: `INSTALL_MASK` for the two session files in `/etc/portage/make.conf`, then re-emerge plasma-login-sessions (the manager or you; agents never run emerge) | — | — | — |
| **M1** | **SDDM Plasma entry gone** | | | **16** |
| PF1 | Power-policy ADR, Settings1 schema, `powerdevilrc` import | `src/services/power_*`, `settings_service` | — | 1 |
| PF2 | Power1 v2: lid, critical battery, per-source profiles, inhibitor registry | `src/services/power_*` | PF1 | 2 |
| PF3 | Idle engine (stages, inhibitors) in desktop-controls | `src/session/idle_policy`, `desktop_controls` | PF2 | 2 |
| PF4 | Native brightness keys and feedback; retire the PowerDevil child, adapters and Settings wiring | `desktop_controls`, `session_supervisor`, `apps/settings/power` | PF2 | 2 |
| PF5 | KWin patch: `ext-session-lock-v1` and the internal lock state | `compositor/patches` | — | 2 |
| PF6 | KWin patch: lock semantics (crash, hotplug, input method, restriction) and the nested lock matrix | `compositor/patches`, `tests/compositor` | PF5 | 2 |
| PF7 | Session-lock Qt integration, `qindaqt-lock` greeter, PAM | new `src/lock_*` | PF5 | 2 |
| PF8 | Lock1 service, ScreenSaver compatibility, logind, settings migration | new `src/services/lock_*`, `session_lock_state`, `apps/settings/screen_lock` | PF2, PF7 | 2 |
| PF9 | M1 packaging: kwin `-lock` build, desktop RDEPENDs, profile; Sloom applets out; depclean; live checks | QindaGentoo | PF1–PF8 | 1 |
| **M2** | **QindaQt's KWin without Plasma** | | | **8** |
| PF10 | KWin `-activities -kcms -runners`; plugin guards | `src/compositor` | — | 1 |
| PF11 | Plasma-free runtime QML (effects off, Outline and OSD QML, tabbox), default-decoration patch | `data/kwin`, `src/session`, `compositor/patches` | — | 2 |
| PF12 | Night-light KWin option patch and QindaQt compositor module | `compositor/patches`, `src/compositor/kwin` | — | 2 |
| PF13 | Night-light schedule authority and Settings migration | `src/services/night_light`, `apps/settings/display` | PF12 | 2 |
| PF14 | M2 packaging: USE flags, profile parent switch, ADR-0098 release contract | QindaGentoo, `compositor/upstream` | PF10–PF13 | 1 |
| **M3** | **Native helpers (fixes live defects)** | | | **4** |
| PF15 | Polkit agent, single-agent rule, profile USE | new `src/apps/polkit_agent`, `session_supervisor` | — | 2 |
| PF16 | `qindaqt-screenshot` and the Print key; drop Spectacle | new `src/apps/screenshot`, `desktop_controls` | — | 2 |
| **M4** | **Native portals** | | | **12** |
| PF17 | Portal foundation; Access, Notification, Inhibit, Email | `src/services/portal` | PF2 | 2 |
| PF18 | FileChooser and AppChooser | `src/services/portal`, file-manager models | PF17 | 3 |
| PF19 | Screenshot and ScreenCast | `src/services/portal` | PF16, PF17 | 3 |
| PF20 | RemoteDesktop, Clipboard, InputCapture over EIS | `src/services/portal` | PF17 | 2 |
| PF21 | Print, Account, DynamicLauncher, Usb, GlobalShortcuts; remove the KDE portal | `src/services/portal`, QindaGentoo | PF17–PF20 | 2 |
| **M5** | **Native global shortcuts** | | | **7** |
| PF22 | Registry core (policy, store, import) | new `src/shortcuts_*` | — | 2 |
| PF23 | In-KWin host (input filter) and `org.kde.kglobalaccel` compatibility | `src/compositor/kwin` | PF22 | 3 |
| PF24 | Shortcuts1, Settings and portal switch-over, KWin `-shortcuts`, drop kglobalacceld | `apps/settings/input`, `session_supervisor`, QindaGentoo | PF21, PF23 | 2 |
| **M6** | **Keep it that way** | | | **1** |
| PF25 | Masks or opt-in sub-profile, final depclean on both machines, docs sweep | QindaGentoo, `docs/wiki` | all | 1 |
| G1 | *Optional, not counted.* SDDM greeter on QindaQt's KWin (Wayland login, no silent X11 fallback) | QindaGentoo | M1 | 1 |

**Lanes.** The compositor lane takes PF5–PF7, then PF10–PF12, then PF22–PF23. The services lane
takes PF1–PF4, then PF8, PF13 and PF15–PF21. The packaging slices (PF9, PF14, PF24's overlay
part, PF25) are manager integration points. M1's two halves are independent until PF8, which needs
PF2's inhibitor registry. The Plasma entry disappears at PF9.

## 5. Gentoo packaging path

- **Keep the overlay's `kde-plasma/kwin`.** At an equal version the overlay's revision wins, and
  `qindaqt-desktop`'s exact `=kwin-6.6.6-rN` dependency keeps ::gentoo's 6.7.5 out. Add the USE flags `activities`, `nightlight` and `plasma` beside the existing `lock` and
  `shortcuts`:
  - `activities? ( plasma-activities )` with `-DKWIN_BUILD_ACTIVITIES=$(usex activities)`
  - `lock? ( kscreenlocker )`; with `-lock` the session-lock patch is active
  - `nightlight? ( knighttime )` with `-DKWIN_BUILD_NIGHTLIGHT=$(usex nightlight)` (patched option)
  - `plasma? ( libplasma milou aurorae breeze )` in RDEPEND, with `-DKWIN_BUILD_KCMS=$(usex plasma)`
    and `-DKWIN_BUILD_RUNNERS=$(usex plasma)`
  - `shortcuts? ( kglobalacceld )` (unchanged)

  Upstream defaults stay on, so a stock build is stock KWin. The QindaQt profile turns them off
  stage by stage, and `gui-wm/qindaqt-desktop` states the result as a USE dependency, e.g.
  `=kde-plasma/kwin-6.6.6-rN:6=[-lock,-activities,-nightlight,-plasma,shortcuts]`.
- **Revisions.** M1 ships kwin `-r3` (session lock), M2 `-r4` (activities, plasma, night light) and
  M5 `-r5` (shortcuts). The tablet-proximity patch from lane T is already in `-r2`.
- **What `qindaqt-desktop` drops from RDEPEND:** kscreenlocker and powerdevil (M1); plasma-activities
  and knighttime (M2); polkit-kde-agent and spectacle (M3); xdg-desktop-portal-kde (M4). It adds
  `sys-auth/polkit-qt`, optional `app-misc/geoclue`, and the PAM file.
- **Profile `qindaqt/systemd`.**
  - `package.use` becomes `kde-plasma/kwin -lock -activities -nightlight -plasma`; the `kwin-x11`
    and `plasma-login-sessions` lines go; add `sys-auth/polkit -gtk -kde` and
    `kde-frameworks/kauth -policykit`.
  - The `packages` set loses polkit-kde-agent, xdg-desktop-portal-kde and the two Sloom applets.
  - The parent moves from `desktop/plasma/systemd` to `desktop/systemd`. That drops the profile USE
    `activities kde kwallet plasma semantic-desktop`; review `emerge -pvuDN @world` on qinda first.
- **Masks (M6).** Add an opt-in `plasma-free` sub-profile, or masks directly in `qindaqt/systemd`
  once qinda's question is settled. It masks plasma-workspace, plasma-login-sessions, kwin-x11,
  kscreenlocker, powerdevil, kactivitymanagerd, knighttime, milou, libplasma, polkit-kde-agent,
  spectacle, xdg-desktop-portal-kde and, after M5, kglobalacceld. A stray dependency then fails at
  emerge time instead of quietly reinstalling Plasma.
- **Rollout per milestone.** Build and test on qinda, then the laptop (`systemd-run -j6`). Check
  that `emerge --depclean -p` lists the expected removals, and look at SDDM.
- **Alternative (not recommended).** A separate `gui-wm/qindaqt-kwin` in a private prefix could
  coexist with a stock Plasma on qinda. It costs about 2 more slices now and effort at every KWin
  upgrade (binary name, plugin paths, D-Bus service files). Only choose it if Plasma stays on qinda
  as a fallback desktop.

## 6. Documentation and ADRs

Every slice updates its wiki pages and runs `./tools/validate-docs`. The manager assigns ADR
numbers when a slice is scheduled:

- QindaQt owns power policy: supersedes
  [ADR-0105](../wiki/adr/0105-delegate-idle-display-off-to-powerdevil.md), and amends
  [ADR-0023](../wiki/adr/0023-split-power-authority-across-service-and-shell.md) and the
  PowerDevil half of [ADR-0132](../wiki/adr/0132-finish-session-locking.md).
- Native lock through a downstream `ext-session-lock-v1` KWin patch: qualifies
  [ADR-0001](../wiki/adr/0001-use-kwin-as-compositor-base.md)'s small-patch rule; supersedes
  [ADR-0091](../wiki/adr/0091-configure-kscreenlocker-preferences-through-settings.md), the lock
  half of ADR-0132 and the greeter-plugin part of
  [ADR-0216](../wiki/adr/0216-the-locker-draws-the-screensaver.md).
- Native night light: supersedes [ADR-0136](../wiki/adr/0136-night-light-through-kwin.md).
- Native polkit agent and the single-agent rule.
- Native screenshot: amends [ADR-0100](../wiki/adr/0100-own-desktop-essentials-in-a-session-process.md).
- Portal families move to `qindaqt`: supersedes rows of
  [ADR-0133](../wiki/adr/0133-route-every-portal-family.md),
  [ADR-0086](../wiki/adr/0086-route-globalshortcuts-only-to-a-verified-backend.md) and
  [ADR-0088](../wiki/adr/0088-enable-kde-remote-desktop-for-qindaqt.md).
- Native shortcut registry: supersedes
  [ADR-0009](../wiki/adr/0009-use-kglobalaccel-for-shell-shortcuts.md).
- The QindaQt KWin build and Plasma-free packaging: updates
  [ADR-0098](../wiki/adr/0098-gate-releases-on-the-exact-native-compositor-stack.md), whose exact
  stack names Plasma Activities.

## 7. Risks

1. **Lock security.** A mistake could show the desktop without a password. Mitigations:
   - KWin's existing enforcement code keeps doing the gating; only the source of "locked" changes.
   - A nested lock matrix and a live checklist.
   - The `lock` USE flag rebuilds the old KScreenLocker path for a one-revision rollback.
2. **Carrying a large KWin patch.** The session-lock patch is about a thousand lines against
   ADR-0001's "small patches", and every KWin upgrade must rebase it. Mitigations: a git-branch
   workflow, and offering it upstream (KWin still lacks the standard protocol).
3. **Laptop power safety.** A bug could leave a lid-closed laptop running in a bag, or let the
   battery hit zero. Mitigations: the lid inhibitor exists only while the policy runs, so logind's
   default returns on a crash; the critical action is tested against a fake UPower; the live check
   runs on battery.
4. **Shortcut compatibility.** Re-creating kglobalacceld's behavior (modifier-only shortcuts,
   releases, keyboard layouts) is the riskiest slice set, and if the registry fails to load, KWin's
   own shortcuts die with it. Mitigations: do it last, and run the registry inactive beside the
   embedded one before flipping the flag.
5. **Portal breadth.** Browsers, Flatpaks and OBS use every corner of these families; a regression
   breaks screen sharing or agent input. Mitigations: flip one routing row at a time, keep the KDE
   fallback until PF21, and rely on the existing real-frontend integration tests.
6. **qinda's deliberate Plasma install.** Both machines share the profile, so the USE and mask
   changes break a Plasma session on qinda (open question 2).
7. **Profile-parent churn.** Changing the parent rebuilds much of @world. Mitigation: do it once, in
   PF14, from a reviewed pretend diff.
8. **Size.** 48 slices is a large program. M1 alone is 16. Each milestone ships on its own and is
   useful without the next one.

## 8. Open questions for the owner

1. **Hide the entry now?** Should we hide the Plasma entries on the laptop right away (M0),
   knowing Plasma stays installed underneath until M1?
2. **Plasma on qinda.** Keep the full Plasma desktop (`@kde-desktop`) on qinda as a fallback? If
   yes, qinda keeps its Plasma entry and needs the separate-prefix KWin package. If no, both
   machines go Plasma-free on the same plan.
3. **Sloom menu add-ons.** The Sloom Studio menu add-ons only work inside a Plasma panel. Can they
   leave the QindaQt package set and your world file? Should QindaQt's own global menu learn
   Sloom Studio's native-Wayland menu route (one extra slice)?
4. **KAuth without polkit.** Is that acceptable? The only loss is KWin's helper for killing a hung
   window owned by root.
5. **KWin's own libraries.** OK to keep kdecoration, kwayland and layer-shell-qt? They are KWin's
   libraries and run nothing. Replacing them would mean forking KWin's decoration and protocol
   libraries.
6. **Tiles editor.** KWin effects that need Plasma's QML get switched off; QindaQt's gather overview
   already replaces the overview grid and present-windows. Do you use KWin's tiles editor
   (Meta+T)?
7. **Wayland login screen.** Should the login screen run on QindaQt's KWin (Wayland) instead of
   silently falling back to X11 at every boot (G1)?
8. **How far to go on shortcuts.** Full replacement (M5, 7 slices, highest compatibility risk), or
   keep KDE's small shortcut library embedded in our KWin, which runs no Plasma service? The plan
   assumes the full replacement, done last.
