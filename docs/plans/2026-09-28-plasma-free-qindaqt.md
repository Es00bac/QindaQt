# Plan: QindaQt without Plasma, on its own KWin fork

- **Date:** 2026-09-28 (revised the same day after the owner's answers to questions 1 and 2)
- **Author:** Claude (Opus 5.5), lane PF, for Jarrod
- **Status:** Proposed. This is a plan only: no code, ebuild or profile has changed.
- **Source base:** container-wm hub `main` at `190e49ba`; QindaGentoo checkout at `69e0ce9`
  (`kde-plasma/kwin-6.6.6-r2`, `gui-wm/qindaqt-desktop-0.1.0_pre20260928-r2`, profile
  `qindaqt/systemd`); the KWin `6.6.6` release tarball; stock KWin's install manifest on qinda
  (`qlist -e kde-plasma/kwin-6.6.6-r1`); the Portage database, journal and running session on
  `qinda-top` on 2026-09-28.
- **Owner's words:**
  - "I want QindaQt to not be tied to any Plasma services; any remaining need their own, native,
    better implementations designed to integrate cleanly in the QindaQt ecosystem."
  - "The goal of this is to completely replace KDE and Plasma … We started with KWin because it was
    much easier than writing a compositor from scratch … modify the things that are there into
    better versions that would actually allow somebody else to install Plasma using the standard
    KWin that's completely unmodified because this is a completely separate environment. It just
    forks a couple things from KWin. And by a couple, I mean a lot."

## The short version

**QindaQt's compositor becomes its own program:** a fork of KWin, renamed from top to bottom
(program, library, plugin folders, settings files, D-Bus names). It installs next to an ordinary
KDE Plasma without touching it, and an ordinary Plasma never touches it. Our overlay stops
patching KDE's KWin. Inside the fork we are free to rework KWin into better versions of itself.

**Why Plasma is on the login screen.** The package that adds that entry comes with Plasma's
workspace package. Three things QindaQt still uses pull that package in: KDE's **lock screen**,
KDE's **power manager** (PowerDevil), and the two **Sloom Studio menu add-ons** for Plasma's panel.
The entry goes away when those are replaced, and it stays visible until then (question 1).

| Step | What you'll notice | Size |
|---|---|---|
| **M1** | **QindaQt's own compositor package.** The desktop runs on the renamed fork. Our settings no longer share files with KDE's, and a stock KWin can sit beside it. Nothing else looks different yet. | 11 slices |
| **M2** | **Plasma gone from the login screen, on both machines.** A new QindaQt lock screen: your screensaver behind the password box, works with touch and the on-screen keyboard, and stays locked even if its window crashes. QindaQt handles lid close, idle dimming and screen-off, the low-battery action and the brightness keys. KDE's locker, PowerDevil, the Sloom Plasma add-ons and qinda's `@kde-desktop` set are uninstalled. | 16 slices |
| **M3** | The fork sheds its Plasma parts: activities, Plasma's theme libraries, Breeze and Aurorae. Night light is run by QindaQt, per screen. | 8 slices |
| **M4** | One password prompt (QindaQt's), never two racing each other. The Print key opens QindaQt's screenshot tool, and Spectacle no longer crashes at logout. | 4 slices |
| **M5** | Open/Save dialogs, "Open with", screen sharing, remote input and app permission prompts are QindaQt's own. KDE's portal, the one that crashes at logout, is removed. | 13 slices |
| **M6** | Keyboard shortcuts are run by a registry built into the fork. Settings shows every shortcut and who owns it, and catches clashes. | 7 slices |
| **M7** | A check fails the build if any QindaQt package ever depends on Plasma again. | 1 slice |

**Total: 60 slices, about 90–150 agent-hours (≈120 at 2 h a slice).**

- Two lanes run side by side: compositor, and services. Elapsed time is about 35 slices, roughly 70
  hours. A third lane on portals shortens that.
- Plasma leaves the login screen after about 14 slices of elapsed time.
- The lock, lid, suspend and battery checks need you on the laptop.

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
set (`plasma-meta`) also installs full Plasma. The owner wants no Plasma fallback, so that set is
dropped at M2 (an owner action; agents never run emerge).

### Inventory

| Package | What QindaQt uses it for | Kind | Without it | Replaced in |
|---|---|---|---|---|
| kwin (overlay, patched, exact 6.6.6) | The compositor. The QindaQt plugin and decoration install into its plugin paths; QindaQt writes `kwinrc`, `kcminputrc` and `kxkbrc`. | compositor | — | M1: the fork `gui-wm/qindaqt-kwin` |
| kdecoration | The decoration API. KWin also links its **private** library (`libkdecorations3private.so.2`), which moves with every Plasma release. | library | — | M1: vendored into the fork |
| plasma-login-sessions | Nothing. Ships the two Plasma session entries. | data | nothing | M2 (goes with plasma-workspace) |
| plasma-workspace | Nothing directly, but today's lock screen is Breeze's `LockScreen.qml`, loaded through its look-and-feel package structure; PowerDevil links its libkworkspace. Its own autostarts are `OnlyShowIn=KDE`. | libraries, data, Plasma session services | the greeter falls back or terminates | M2 |
| kscreenlocker | Linked into KWin (`lock` USE): lock state, `org.freedesktop.ScreenSaver` and `org.kde.screensaver` (lock, inhibit, active), auto-lock timer, lock on resume, the `kscreenlocker_greet` process. Settings writes `kscreenlockerrc` (ADR-0091, 0132); the screensaver is drawn by a Plasma wallpaper plugin (ADR-0216). | service inside `kwin_wayland` plus greeter process | no lock screen; apps can't hold the screen on through ScreenSaver; the notification gate (ADR-0011) loses lock truth | PF5–PF8 |
| powerdevil | Supervisor child `org_kde_powerdevil`: idle display-off (ADR-0105), lid and power policy (ADR-0132), per-source power profile, brightness keys and `org.kde.ScreenBrightness` with feedback, critical-battery action, power-management inhibitors. | runtime service | lid close gets logind's default; no idle screen-off; brightness keys dead; **no critical-battery action** | PF1–PF4 |
| sloom-globalmenu, sloom-panelmenu (overlay) | Nothing in QindaQt: Plasma applets only run inside `plasmashell`, and QindaQt has its own global menu (`src/shell/global_menu`). | Plasma applets | nothing in QindaQt | PF9 (question 3) |
| kwin-x11 | Nothing. Pulled by `plasma-login-sessions[X]`. | Plasma X11 compositor | nothing | M2 |
| kactivitymanagerd | Nothing. D-Bus-activated by KWin's activities support; running in the live session. | service | nothing | PF10 |
| plasma-activities | KWin build option. The QindaQt plugin includes KWin's `activities.h` in two files and finds the package in `src/compositor/CMakeLists.txt`; exact RDEPEND (ADR-0098); `kio-extras[activities]` through the Plasma profile's USE. | library | plugin needs two guards | PF10, PF14 |
| libplasma | KWin's runtime QML (overview, window view, tiles editor, desktop-change OSD, output locator, effect frames, snap outline, on-screen notification, thumbnail-grid switcher). QindaQt's own tabbox uses `PlasmaCore.Dialog`. Also pulled by polkit-kde-agent, kscreenlocker, powerdevil, kirigami-addons and the Sloom applets. | QML runtime library | those QML scenes fail to load | PF11 (M2 and M4 remove the other users) |
| milou | KWin's overview search only. | QML | overview fails to load | PF11 |
| aurorae, breeze | KWin's hard-coded default (`org.kde.breeze`) and fallback (`org.kde.kwin.aurorae`) decoration. Settings lists them only when installed (ADR-0160). | plugins | nothing while QindaQt's decoration loads | F6 (the fork defaults to QindaQt's decoration); PF11 drops the packages |
| knighttime | Required to build KWin's night light plugin; `knighttimed` computes the schedule (running). Settings writes `knighttimerc` and `kwinrc [NightColor]` (ADR-0136). | service and library | KWin will not configure without a change | PF12–PF13 |
| kglobalacceld | Embedded in `kwin_wayland` as `org.kde.kglobalaccel`: every KWin, shell, desktop-controls and app shortcut, including "Meta alone". The supervisor also starts `/usr/libexec/kglobalacceld`, but no such process runs in the live session (KWin already owns the name, so it presumably exits). | library inside KWin | every keyboard shortcut | PF22–PF24 |
| xdg-desktop-portal-kde | 16 portal families: FileChooser, AppChooser, Access, Email, Inhibit, Notification, Print, Screenshot, ScreenCast, RemoteDesktop, GlobalShortcuts, InputCapture, Clipboard, Usb, Account, DynamicLauncher. The laptop has no gtk or lxqt portal, so there `kde` is the only real backend behind `kde;gtk;lxqt` (qinda has both, from its LXQt set). Reported to crash at logout. | service | file dialogs, screen sharing, agent input (ADR-0087, 0088) | PF17–PF21 |
| spectacle | The Print key (`desktop-controls` launches it). Pulls kpipewire. Reported to crash at logout. | application | Print does nothing | PF16 |
| polkit-kde-agent | Supervisor child. Also a PDEPEND of `kauth[policykit]`. Pulls libplasma. | service | no privilege prompts | PF15 |
| polkit-gnome (not Plasma) | XDG autostart (`NotShowIn=MATE;KDE`) runs it in QindaQt too, so it **races the supervisor's KDE agent at every login**: polkitd's log shows the KDE agent winning one session and the GNOME agent the next. Pulled by `sys-auth/polkit[gtk]`. | service | — | PF15 |
| libkscreen, kirigami-addons, kpipewire, kde-cli-tools | Only through the packages above, or the Plasma profile's `plasma` USE (`xdg-utils[plasma]`). | libraries, tools | — | M2, M4, PF14 |

**Shared, not forked:**
- **kwayland**: a stable client library (`libKWaylandClient.so.6`), used by the fork's nested
  Wayland backend and by desktop-controls.
- **layer-shell-qt**: a stable client library for the standard layer-shell protocol, used by the
  shell and screensavers.

Both keep their SONAME across Plasma releases and are independent of the compositor's version, so
sharing them does not tie the fork to Plasma's release cadence. Fork one only when it stops
building against the fork. KDE Frameworks (KConfig, the KGlobalAccel client, KIdleTime, KIO…) are
not Plasma and stay shared. gnome-keyring stays the Secret provider.

**Other live finding.** The SDDM greeter is configured for Wayland (`01gentoo.conf`) but has no
compositor, so every boot logs `HELPER_DISPLAYSERVER_ERROR` and falls back to X11 (optional slice
G1).

## 2. QindaQt's KWin: a co-installable fork

### 2.1 Where the fork lives

- **Repository.** A new hub, `qinda:~/git/qindaqt-kwin.git` (checkout `~/work_SPaC3/qindaqt-kwin`),
  created from KWin's git history at `v6.6.6`: tag object `43cb730c`, commit `9bf2235f`, the
  values already pinned in `compositor/upstream/kwin.json`. Keeping upstream history makes later
  merges ordinary `git merge`s. It gets its own `AGENTS.md`: QindaQt commits are small,
  self-contained and labelled, and every upstream merge is recorded.
- **Working name.** `qindaqt-kwin` (question 9). It must not be `qindaqt-compositor`: container-wm
  already uses the target `qindaqt_compositor` and the D-Bus service `org.qindaqt.Compositor` for
  its KWin plugin.
- **The two pinned patches become commits.** The corner-tab cutout (`0001`, ADR-0277, touches
  `src/window.cpp`) and tablet proximity (`0002`, lane T, touches `src/input.cpp`) are applied with
  `git am` as the fork's first QindaQt commits, keeping their messages.
  - In container-wm, `compositor/patches/` and `series.json` retire.
  - `compositor/upstream/kwin.json` is rewritten to name the fork: repository, exact commit, fork
    version, and upstream base tag.
  - `verify-kwin-source` checks that the fork commit descends from `9bf2235f` and matches the
    packaged tarball.
- **The overlay stops patching KDE's KWin.** `kde-plasma/kwin` and its patch files leave
  QindaGentoo (F5). Stock KWin then comes only from ::gentoo, unmodified, and only if someone
  installs Plasma.
- **Upstream tracking.** The fork stays on the 6.6 line. It merges upstream bug-fix tags when they
  matter, re-running the rename script (F2) on the merged tree. Moving to KWin 6.7 or later is a
  separate, estimated project.

### 2.2 The collision surface and the fork's identity

Stock `kde-plasma/kwin-6.6.6-r1` installs **500 files** on qinda. Every one of them is renamed or
dropped, so no file overlaps:

| Stock KWin 6.6.6 | Files | Fork |
|---|---|---|
| `/usr/bin/kwin_wayland`, `kwin_wayland_wrapper`, `kwindowprop` | 3 | `/usr/bin/qindaqt-kwin` (plus a wrapper only if the launcher needs one); `kwindowprop` dropped |
| `/usr/libexec/kwin_killer_helper`, `kwin-tabbox-preview`, `kwin-applywindowdecoration` | 3 | `/usr/libexec/qindaqt-kwin/killer-helper`; the two KCM helpers dropped |
| `libkwin.so.6` (+links), `libkcmkwincommon.so.6` (+link) | 5 | `libqindaqt-kwin.so.0` (SONAME `libqindaqt-kwin.so.0`); the KCM library dropped. The C++ namespace `KWin::` stays: the two libraries never load in one process, and keeping it keeps upstream merges tractable. |
| `/usr/lib64/kconf_update_bin/kwin*` and `/usr/share/kconf_update/kwin.upd` | 7 | dropped (a fresh config namespace needs no legacy migrations) |
| `/usr/include/kwin/` | 287 | `/usr/include/qindaqt-kwin/` |
| CMake `KWin`, `KWinDBusInterface` | 6 | `QindaQtKWin`, with the fork version as the exact package version |
| `qt6/plugins/kwin/effects`, `kwin/plugins` | 27 | `qt6/plugins/qindaqt-kwin/effects`, `…/plugins`, plus `…/decorations`. New plugin IID `org.qindaqt.kwin.PluginFactoryInterface<version>`, so stock plugins never load in the fork and fork plugins never load in stock KWin. |
| `qt6/plugins/plasma/kcms` | 12 | not built (`KWIN_BUILD_KCMS=OFF`; QindaQt Settings replaces them) |
| `qt6/plugins/kf6/packagestructure/kwin_*` | 5 | `qindaqt_kwin_*` with structure ids `QindaQtKWin/Effect`, `/Script`, `/WindowSwitcher`, `/Decoration`; Aurorae's structure dropped |
| `qt6/qml/org/kde/kwin/` | 10 | `qt6/qml/org/qindaqt/kwin/`; QML URIs `org.kde.kwin*` become `org.qindaqt.kwin*` |
| `/usr/share/kwin-wayland/` (effects, builtin effects, scripts, tabbox, frames, outline, OSD) | 98 | `/usr/share/qindaqt-kwin/` |
| `/usr/share/dbus-1/interfaces/org.kde.KWin*.xml`, `org.kde.kwin.*.xml` | 9 | renamed `org.qindaqt.KWin*.xml` |
| `/usr/lib/systemd/user/plasma-kwin_wayland.service` | 1 | none (the QindaQt supervisor starts the compositor) |
| `/usr/share/applications/` (killer helper plus 10 KCMs) | 11 | `org.qindaqt.kwin.killer.desktop` only |
| `config.kcfg` (4), `knsrcfiles` (4), `krunner/dbusplugins` (1), hicolor icons (4), handbook (1) | 14 | not installed (KCMs, KNewStuff and KRunner off; kcfg is compiled in) |
| `knotifications6/kwin.notifyrc`, `qlogging-categories6/org_kde_kwin.categories` | 2 | `qindaqt-kwin.notifyrc`, `qindaqt-kwin.categories` |

Names that are not files but must still separate:

- **Config** (`$XDG_CONFIG_HOME`): `kwinrc`, `kwinrulesrc`, `kwinoutputconfig.json`, `kcminputrc`
  (the `[Libinput]` groups) and `kxkbrc` become `qindaqt-kwinrc`, `qindaqt-kwinrulesrc`,
  `qindaqt-kwinoutputconfig.json`, `qindaqt-kwininputrc` and `qindaqt-kwinxkbrc`.
  - The QindaQt session imports each one once from the KDE file, so your monitor layout, window
    rules, input and keyboard settings carry over.
  - `kdeglobals` stays shared; it belongs to KDE Frameworks, not KWin.
  - `kglobalshortcutsrc`, `kscreenlockerrc` and `knighttimerc` leave with their services (M6, M2,
    M3).
- **D-Bus:** the service `org.kde.KWin` becomes `org.qindaqt.KWin`; object paths move under
  `/org/qindaqt/KWin/`; interfaces `org.kde.KWin.*` and `org.kde.kwin.*` become
  `org.qindaqt.KWin.*`.
  - Carve-out, until PF21: the fork also owns `org.kde.KWin` and keeps, under their old names, the
    objects that the KDE portal and Spectacle still call. These are ScreenShot2, the EIS remote
    desktop and input capture interfaces, and the TabletModeManager and VirtualKeyboard interfaces
    named in the portal binary. QindaQt's own callers of those (gather previews, the screenshot
    tool) switch in PF21 together with the rename.
  - Unchanged on purpose: `org.kde.kglobalaccel` (the KF6GlobalAccel protocol, until M6),
    `org.freedesktop.*`, and every Wayland protocol name (`org_kde_*`, `zkde_*`, `wp_*`, `xdg_*`,
    `ext_*`). Those are wire contracts that clients such as layer-shell-qt and kwayland depend on.
- **Restricted-interface keys:** the fork reads `X-QindaQt-KWin-Wayland-Interfaces` and
  `X-QindaQt-KWin-DBus-Restricted-Interfaces` from a client's desktop file. It also honours the
  `X-KDE-*` keys only until PF21, so afterwards stock KDE apps get no privileged access in a
  QindaQt session.
- **Translation domain:** `kwin` becomes `qindaqt-kwin`. No catalogs are shipped for now (the
  profile's L10N is English), and stock `kwin.mo` is never picked up.
- **Environment knobs:** the `KWIN_*` debug variables stay as they are; they are not files.

Proof of separation (F8):
- `tools/check-install-collisions` in the fork compares its install manifest with stock
  `kde-plasma/kwin` and `kwin-x11`, and fails on any overlap.
- On qinda, with both installed, the QindaQt session runs on the fork, stock `kwin_wayland --virtual`
  still starts, and a sandboxed `$HOME` shows QindaQt never creating `kwinrc`, `kcminputrc` or
  `kxkbrc`.

### 2.3 How QindaQt targets the fork

- **Compositor plugin.** It stays in container-wm (`src/compositor`), built against the fork's
  installed CMake package `QindaQtKWin` at the exact fork version (the ADR-0098 contract moves from
  stock KWin to the fork). It gets the new IID and installs to `qindaqt-kwin/plugins`. The two
  `activities.h` users are guarded in PF10.
- **Decoration plugin.** `src/decorations` links the fork's vendored decoration library and
  installs to `qindaqt-kwin/decorations`. The fork's decoration bridge reads only that namespace
  and defaults and falls back to QindaQt's decoration, so Breeze and Aurorae are never needed.
  ADR-0160's chooser lists only fork-namespace decorations.
- **Tabbox.** `data/kwin/tabbox/qindaqt` installs to `/usr/share/qindaqt-kwin/tabbox/`, uses the
  `QindaQtKWin/WindowSwitcher` structure, and imports `org.qindaqt.kwin`.
- **Session.**
  - `qindaqt-wm` (`kwincommandbuilder`) and the supervisor's activation policy expect
    `qindaqt-kwin`. The launcher's `--help` capability probe stays.
  - The session defaults, window-settings bridge (ADR-0209), overview edge, input, keyboard layout,
    tablet mapping (the InputDevice D-Bus), display inventory and Settings routes move to the
    fork's config and D-Bus names. That is 27 source files naming `kwinrc`, 13 naming
    `kcminputrc`/`kxkbrc` and 17 naming `org.kde.KWin`, plus about 50 test files.
  - The nested harness makes its no-caps copy from the fork binary.
- **Where new compositor work goes.** Changes that need KWin internals become fork commits: the
  lock protocol, night light and the shortcut registry. QindaQt policy that fits the plugin API
  stays in container-wm. Folding the plugin into the fork is possible later, but not planned.

### 2.4 Shared libraries

- **kdecoration: forked (vendored into the fork) in F4.** KWin links kdecoration's private library,
  whose SONAME moves with Plasma releases. Sharing it would chain the fork to stock Plasma's release
  cadence and block a stock Plasma upgrade on the same machine. It becomes
  `libqindaqt-kwin-decorations.so.0` (and its private part), with headers under
  `/usr/include/qindaqt-kwin/decoration/` and CMake `QindaQtKWinDecoration`.
- **kwayland and layer-shell-qt: shared.** They carry a range dependency and a subslot rebuild; see
  §1.
- **kglobalacceld and kscreenlocker:** linked by the fork only until M6 and M2 respectively.

### 2.5 Shedding Plasma inside the fork

These are fork commits or fixed build options, not USE flags:

| Dependency | How KWin 6.6.6 wires it | In the fork | Slice |
|---|---|---|---|
| KCMs, KRunner integration, handbook | `KWIN_BUILD_KCMS`, `KWIN_BUILD_RUNNERS` | OFF from the first build (they would collide) | F2 |
| kdecoration | REQUIRED, including the private library | vendored | F4 |
| breeze, aurorae | RUNTIME under `KWIN_BUILD_DECORATIONS` | keep the option ON (OFF also flips the default placement to Maximizing, `src/kwin.kcfg`); the bridge defaults to QindaQt's decoration | F6, PF11 |
| kscreenlocker | REQUIRED when `KWIN_BUILD_SCREENLOCKER` | KSldApp integration replaced by `ext-session-lock-v1` | PF5–PF6 |
| plasma-activities, kactivitymanagerd | OPTIONAL, `KWIN_BUILD_ACTIVITIES` | OFF | PF10 |
| libplasma, milou | RUNTIME QML | effects QindaQt already replaces are deleted (overview, window view, desktop-change OSD, thumbnail-grid switcher, output locator); outline, OSD and frames (and the tiles editor if kept, question 6) are ported to QtQuick/Kirigami | PF11 |
| knighttime | REQUIRED, no option (`src/plugins/nightlight`) | the fork's night light takes its schedule from QindaQt instead of KNightTime | PF12 |
| kglobalacceld | REQUIRED when `KWIN_BUILD_GLOBALSHORTCUTS`; embedded in `GlobalShortcutsManager` | a native registry in `GlobalShortcutsManager`, keeping `org.kde.kglobalaccel` compatibility | PF22–PF24 |

Traps a future agent must know:

- With KGlobalAccelD removed, `GlobalShortcutsManager::processKey` dispatches no keyboard shortcuts,
  KWin's own included, until the native registry is wired in. Both land in the same fork revision.
- `--no-lockscreen` and `--no-kactivities` exist only when those features are built. Keep the
  launcher's probe, and never pass them from an SDDM greeter command.
- KWin 6.6.6 has no `ext-session-lock-v1` (searched the tree), so the native lock is new fork code.
- The effect-frame QML path (`frames/plasma`) is hard-coded but only the mouse-click effect uses it.

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
  Wayland client that runs the screensaver launcher). It uses `ext-idle-notify-v1` stages:
  screensaver, dim, display off (`org_kde_kwin_dpms`), lock (Lock1), suspend (Power1). Stages are
  suppressed while Power1 reports an idle inhibition; Wayland surface inhibitors are honored by the
  protocol itself.
- **Brightness.** The brightness keys call Power1 `SetInternalBrightness` (ADR-0148) and Display1
  `SetOutputBrightness` (ADR-0150); the keyboard backlight goes through Power1 and UPower; feedback
  uses the existing notifier.
- **Preferences** move to Settings1 (`power.lid.*`, `power.idle.*`, `power.critical.*`,
  `power.profile.*`), imported once from `powerdevilrc`. The three `powerdevil_*` adapters and the
  supervisor's PowerDevil child are deleted.
- **Tests.** Fake logind, UPower and power-profiles buses (the PB-2 pattern); invariant tests for
  the lid-inhibitor lifetime and the critical action; unit tests for the idle-stage state machine.

### 3.2 Lock screen, replacing KScreenLocker (PF5–PF8)

- **Compositor (fork commits).** Replace the KSldApp integration (the `#if KWIN_BUILD_SCREENLOCKER`
  sites in 11 files) with the standard `ext-session-lock-v1` protocol and a small internal lock
  state. KWin's own enforcement stays in charge: `isScreenLocked()`, `Window::isLockScreen()`, the
  input filters, effects, screen edges and input method. Protocol rules:
  - `locked` is sent only after every output has shown a lock surface (the
    `LockScreenPresentationWatcher` logic).
  - If the locker dies, the session stays locked on a solid fallback, and a restarted locker can
    take over.
  - Only QindaQt's locker may bind the global (restricted-interface key).
- **Greeter (`qindaqt-lock`).** QindaTK and QML, one lock surface per output through a small Qt
  shell-integration plugin for session-lock surfaces, modeled on LayerShellQt. It shows the clock,
  the user, the password field, the keyboard layout and the on-screen keyboard (the fork's input
  method is allowed on lock surfaces), and it works with touch. It draws the chosen screensaver's
  QML item behind the prompt, using the same Qinda.Patrol and CircuitReef modules; the Plasma
  wallpaper plugin `studio.qinda.screensaver` goes away. The PAM service `qindaqt-lock` is
  installed by the ebuild.
- **Service (`Lock1`).** `org.qindaqt.Lock1` plus `org.freedesktop.ScreenSaver` compatibility (Lock,
  GetActive, GetActiveTime, SetActive, Inhibit and UnInhibit forwarded to Power1, ActiveChanged). It
  handles logind's Lock and Unlock and takes a `PrepareForSleep` delay inhibitor, so the lock is on
  screen before suspend. It also sets `LockedHint`, keeps the grace period and restarts the greeter.
  `session_lock_state` (the ADR-0011 notification gate) and `session_actions` keep working through
  the compatibility name, then move to Lock1.
- **Settings.** The Screen lock and Screen saver routes move from `kscreenlockerrc` to Settings1
  `lock.*`, imported once.
- **Tests.** A nested fork matrix: keyboard, pointer and touch never reach a normal client while
  locked; a locker crash keeps the lock; hotplug while locked; unlock only after PAM success;
  lock-before-sleep ordering; the on-screen keyboard on a lock surface.

### 3.3 Night light, replacing KNightTime (PF12–PF13)

- **Compositor.** The fork's own night light plugin keeps applying colour temperature per output
  (`Output::setChannelFactors`), but it drops KNightTime: schedule targets arrive from QindaQt over
  `org.qindaqt.KWin`. It gains a per-output opt-out, for example a calibrated external monitor (it
  composes with the `display_color_*` modules), and a temporary inhibit for colour picking and
  screenshots.
- **Schedule.** The existing `night_light` service becomes the schedule authority: sunrise and
  sunset from a manual location or GeoClue2, or fixed times, plus the transition length. Settings
  moves to Settings1 `display.nightLight.*`, imported once from `[NightColor]` and
  `knighttimerc`. This supersedes ADR-0136.

### 3.4 Polkit agent (PF15)

- `qindaqt-polkit-agent` is built on polkit-qt6 (`sys-auth/polkit-qt`, not Plasma). Its QindaTK
  dialog offers the admin identity choice, the password, retry and cancel, and the action's details
  in plain words, with keyboard, touch and screen-reader support.
- The supervisor starts it in place of the KDE candidates. The autostart runner (ADR-0247) skips
  other agents (polkit-gnome, polkit-kde, lxqt-policykit, mate-polkit), so exactly one registers.
- Profile changes: `sys-auth/polkit -gtk -kde`, and `kde-frameworks/kauth -policykit`, whose
  PDEPEND pulls polkit-kde-agent and so libplasma. KWin's only KAuth use is the helper that kills a
  hung window owned by root (question 4).

### 3.5 Screenshot (PF16)

- `qindaqt-screenshot` captures through the fork's ScreenShot2, which the shell already uses for
  gather previews. It takes the full screen, the active window, a picked window or a region (a
  layer-shell overlay over a frozen frame, usable from the keyboard), with a delay option.
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
  - Screenshot and ScreenCast, over the fork's `zkde_screencast_unstable_v1` and PipeWire.
  - RemoteDesktop, Clipboard and InputCapture, over the fork's EIS. This ends ADR-0088's
    `XDG_CURRENT_DESKTOP=KDE` workaround and keeps ADR-0087's agent input.
  - Print, Account, DynamicLauncher and Usb.
  - GlobalShortcuts, over the kglobalaccel API until M6 and over Shortcuts1 after it.
- **PF21 also ends the carve-outs.** The fork drops `org.kde.KWin` and the `X-KDE-*` keys, and the
  carved-out interfaces are renamed together with QindaQt's callers.
- Background and Wallpaper stay closed, and Secret stays with gnome-keyring.

### 3.7 Global shortcuts, replacing kglobalacceld (PF22–PF24)

- **Inside the fork.** The registry lives inside the fork's `GlobalShortcutsManager`, where key
  interception is already synchronous with input. It handles press, release and repeat,
  modifier-only shortcuts ("Meta alone") and layout-independent matching, and respects the lock
  screen and the keyboard-shortcuts-inhibitor protocol. The policy and store are a small pure
  library with its own tests.
- **Compatibility contract.** It owns `org.kde.kglobalaccel` (the `org.kde.KGlobalAccel` and
  `org.kde.kglobalaccel.Component` interfaces), so KWin's own actions and every KF6GlobalAccel
  client (shell, desktop-controls, KDE apps) keep working unchanged. `kglobalshortcutsrc` is
  imported once into the fork's own store.
- **Native interface.** `org.qindaqt.Shortcuts1` serves Settings → Input (one list, the owner of
  each key, clashes shown before saving) and the GlobalShortcuts portal. The supervisor drops its
  kglobalacceld child.

## 4. Slices and order

| # | Outcome | Main paths | Needs | Slices |
|---|---|---|---|---|
| **M1** | **QindaQt's own compositor package (the fork's identity)** | | | **11** |
| F1 | Fork repository from `v6.6.6`; `0001` and `0002` as commits; fork ADR | new `qindaqt-kwin` repo | — | 1 |
| F2 | Scripted identity rename, part 1: binaries, library and SONAME, headers, CMake package, plugin namespaces and IID, package structures, QML URIs, data directory; drop KCMs, runners, handbook, systemd unit, kconf_update, KNewStuff | `qindaqt-kwin` | F1 | 2 |
| F3 | Identity rename, part 2: config names, D-Bus names with carve-outs, notifyrc, logging categories, translation domain, restricted-interface keys | `qindaqt-kwin` | F2 | 1 |
| F4 | Vendor kdecoration as the fork's decoration library | `qindaqt-kwin` | F2 | 1 |
| F5 | Collision check; `gui-wm/qindaqt-kwin` ebuild; remove the overlay's `kde-plasma/kwin` | `qindaqt-kwin`, QindaGentoo | F3, F4 | 1 |
| F6 | QindaQt compositor side: plugin, decoration and tabbox against the fork; manifest, verify and release-contract tools; nested harness | `src/compositor`, `src/decorations`, `data/kwin`, `compositor/`, `tools/` | F3, F4 | 2 |
| F7 | QindaQt session and services: launcher, supervisor, config names with one-time import, D-Bus names, Settings routes, tests, wiki | `src/session*`, `src/services`, `src/apps/settings` | F3 | 2 |
| F8 | Co-installation proof and rollout on both machines | QindaGentoo | F5–F7 | 1 |
| **M2** | **Plasma gone from the login screen** | | | **16** |
| PF1 | Power-policy ADR, Settings1 schema, `powerdevilrc` import | `src/services/power_*`, `settings_service` | — | 1 |
| PF2 | Power1 v2: lid, critical battery, per-source profiles, inhibitor registry | `src/services/power_*` | PF1 | 2 |
| PF3 | Idle engine (stages, inhibitors) in desktop-controls | `src/session/idle_policy`, `desktop_controls` | PF2 | 2 |
| PF4 | Native brightness keys and feedback; retire the PowerDevil child, adapters and Settings wiring | `desktop_controls`, `session_supervisor`, `apps/settings/power` | PF2 | 2 |
| PF5 | Fork: `ext-session-lock-v1` and the internal lock state, replacing KSldApp | `qindaqt-kwin` | F3 | 2 |
| PF6 | Fork: lock semantics (crash, hotplug, input method, restriction) and the nested lock matrix | `qindaqt-kwin`, `tests/compositor` | PF5 | 2 |
| PF7 | Session-lock Qt integration, `qindaqt-lock` greeter, PAM | new `src/lock_*` | PF5 | 2 |
| PF8 | Lock1 service, ScreenSaver compatibility, logind, settings migration | new `src/services/lock_*`, `session_lock_state`, `apps/settings/screen_lock` | PF2, PF7 | 2 |
| PF9 | Packaging: the fork without kscreenlocker, desktop RDEPENDs, profile; Sloom applets and qinda's `@kde-desktop` out; depclean; live checks | QindaGentoo | F8, PF1–PF8 | 1 |
| **M3** | **The fork without Plasma parts** | | | **8** |
| PF10 | Fork without activities; plugin guards | `qindaqt-kwin`, `src/compositor` | F6 | 1 |
| PF11 | Plasma-free QML: delete replaced effects, port outline, OSD and frames; drop libplasma, milou, breeze, aurorae | `qindaqt-kwin`, `data/kwin`, `src/session` | F6 | 2 |
| PF12 | Fork night light without KNightTime; per-output opt-out; schedule over D-Bus | `qindaqt-kwin` | F3 | 2 |
| PF13 | Night-light schedule authority and Settings migration | `src/services/night_light`, `apps/settings/display` | PF12 | 2 |
| PF14 | Packaging: the fork's dependencies shrink; profile parent switch | QindaGentoo | PF10–PF13 | 1 |
| **M4** | **Native helpers (fixes live defects)** | | | **4** |
| PF15 | Polkit agent, single-agent rule, profile USE | new `src/apps/polkit_agent`, `session_supervisor` | — | 2 |
| PF16 | `qindaqt-screenshot` and the Print key; drop Spectacle | new `src/apps/screenshot`, `desktop_controls` | F7 | 2 |
| **M5** | **Native portals** | | | **13** |
| PF17 | Portal foundation; Access, Notification, Inhibit, Email | `src/services/portal` | PF2 | 2 |
| PF18 | FileChooser and AppChooser | `src/services/portal`, file-manager models | PF17 | 3 |
| PF19 | Screenshot and ScreenCast | `src/services/portal` | PF16, PF17 | 3 |
| PF20 | RemoteDesktop, Clipboard, InputCapture over EIS | `src/services/portal` | PF17 | 2 |
| PF21 | Print, Account, DynamicLauncher, Usb, GlobalShortcuts; remove the KDE portal; end the D-Bus and `X-KDE-*` carve-outs | `src/services/portal`, `qindaqt-kwin`, QindaGentoo | PF17–PF20 | 3 |
| **M6** | **Native global shortcuts** | | | **7** |
| PF22 | Registry core: policy, store, import | `qindaqt-kwin` | F3 | 2 |
| PF23 | Registry in `GlobalShortcutsManager`, `org.kde.kglobalaccel` compatibility, KGlobalAccelD removed | `qindaqt-kwin` | PF22 | 3 |
| PF24 | Shortcuts1, Settings and portal switch-over; drop kglobalacceld | `apps/settings/input`, `session_supervisor`, QindaGentoo | PF21, PF23 | 2 |
| **M7** | **Keep it that way** | | | **1** |
| PF25 | Dependency guard, optional per-machine opt-out, final depclean on both machines, docs sweep | QindaGentoo, `docs/wiki` | all | 1 |
| G1 | *Optional, not counted.* SDDM greeter on the fork (Wayland login, no silent X11 fallback) | QindaGentoo | M2 | 1 |

**Lanes:**

| Lane | Slices, in order | Total |
|---|---|---|
| Compositor | F1, F2, F3, F4, F6, then PF5, PF6, PF7, then PF10, PF11, PF12, then PF22, PF23 | 23 |
| Services | PF1–PF4, F7, PF8, then PF13, PF15, PF16, PF17–PF21, PF24 | 32 |
| Manager integration | F5, F8, PF9, PF14, PF25 | 5 |

The Plasma entry disappears at PF9. Its critical path is the compositor lane through PF7 (13
slices) plus PF9.

## 5. Gentoo packaging path

- **New package `gui-wm/qindaqt-kwin`** in QindaGentoo.
  - Version `6.6.6_pN`: the upstream base plus a fork serial. The CMake package version is
    `6.6.6.N`.
  - The source is a `git archive` tarball of the fork's hub commit, with the same `pkg_nofetch`
    pattern as `qindaqt-desktop`.
  - `FILECAPS` gives `cap_sys_nice` to `/usr/bin/qindaqt-kwin`.
  - It blocks nothing and shares no files, so stock `kde-plasma/kwin` and Plasma stay installable
    beside it.
- **`gui-wm/qindaqt-desktop`** replaces `=kde-plasma/kwin-6.6.6-r1:6=[lock,shortcuts]` and
  `=kde-plasma/kdecoration-6.6.6*` with `=gui-wm/qindaqt-kwin-6.6.6_pN:=`, and keeps
  `kde-plasma/kwayland` and `kde-plasma/layer-shell-qt` as range dependencies.
  - It drops kscreenlocker and powerdevil (M2), plasma-activities and knighttime (M3),
    polkit-kde-agent and spectacle (M4), and xdg-desktop-portal-kde (M5).
  - It adds `sys-auth/polkit-qt`, optional `app-misc/geoclue`, and the PAM file.
- **The fork's own dependencies shrink by release:** kscreenlocker (M2); plasma-activities,
  knighttime, libplasma, milou, breeze and aurorae (M3); kglobalacceld (M6).
- **QindaGentoo loses** `kde-plasma/kwin` and its patches (F5), and the `kde-plasma/plasma-activities`
  pin (M3).
- **Profile `qindaqt/systemd`.**
  - `package.use` loses its `kde-plasma/kwin`, `kwin-x11` and `plasma-login-sessions` lines; M4
    adds `sys-auth/polkit -gtk -kde` and `kde-frameworks/kauth -policykit`.
  - The `packages` set loses polkit-kde-agent, xdg-desktop-portal-kde and the two Sloom applets.
  - The parent moves from `desktop/plasma/systemd` to `desktop/systemd` in PF14. That drops the
    profile USE `activities kde kwallet plasma semantic-desktop`; review `emerge -pvuDN @world` on
    qinda first.
- **No global Plasma masks.** Plasma must stay installable next to QindaQt, so PF25 adds a
  dependency guard instead: a check on the resolved dependency graphs of `qindaqt-desktop` and
  `qindaqt-kwin` that fails on any `kde-plasma/*` package outside an allow-list (kwayland,
  layer-shell-qt, and kscreenlocker or kglobalacceld only until M2 or M6). A machine that should
  never get Plasma can opt into a `plasma-free` sub-profile or its own `/etc/portage/package.mask`.
- **Rollout per milestone.** Build and test on qinda, then the laptop (`systemd-run -j6`). Check
  that `emerge --depclean -p` lists the expected removals, and look at SDDM.

## 6. Documentation and ADRs

Every slice updates its wiki pages (`compositor-session`, `module-boundaries`, the testing harness,
the KWin-upgrade procedure, releases) and runs `./tools/validate-docs`. The manager assigns ADR
numbers when a slice is scheduled:

- **QindaQt runs on its own KWin fork:** supersedes
  [ADR-0001](../wiki/adr/0001-use-kwin-as-compositor-base.md)'s small-downstream-patch model and
  updates [ADR-0098](../wiki/adr/0098-gate-releases-on-the-exact-native-compositor-stack.md)'s
  exact stack (the fork replaces stock KWin, kdecoration and Plasma Activities). It amends
  [ADR-0160](../wiki/adr/0160-select-installed-kwin-window-decorations.md) (fork-namespace
  decorations) and moves ADR-0277's patch into the fork.
- **QindaQt owns power policy:** supersedes
  [ADR-0105](../wiki/adr/0105-delegate-idle-display-off-to-powerdevil.md), and amends
  [ADR-0023](../wiki/adr/0023-split-power-authority-across-service-and-shell.md) and the
  PowerDevil half of [ADR-0132](../wiki/adr/0132-finish-session-locking.md).
- **Native lock through `ext-session-lock-v1` in the fork:** supersedes
  [ADR-0091](../wiki/adr/0091-configure-kscreenlocker-preferences-through-settings.md), the lock
  half of ADR-0132 and the greeter-plugin part of
  [ADR-0216](../wiki/adr/0216-the-locker-draws-the-screensaver.md).
- **Native night light:** supersedes [ADR-0136](../wiki/adr/0136-night-light-through-kwin.md).
- **Native polkit agent** and the single-agent rule.
- **Native screenshot:** amends
  [ADR-0100](../wiki/adr/0100-own-desktop-essentials-in-a-session-process.md).
- **Portal families move to `qindaqt`:** supersedes rows of
  [ADR-0133](../wiki/adr/0133-route-every-portal-family.md),
  [ADR-0086](../wiki/adr/0086-route-globalshortcuts-only-to-a-verified-backend.md) and
  [ADR-0088](../wiki/adr/0088-enable-kde-remote-desktop-for-qindaqt.md).
- **Native shortcut registry:** supersedes
  [ADR-0009](../wiki/adr/0009-use-kglobalaccel-for-shell-shortcuts.md).

## 7. Risks

1. **Lock security.** A mistake could show the desktop without a password. Mitigations:
   - KWin's existing enforcement code keeps doing the gating; only the source of "locked" changes.
   - A nested lock matrix and a live checklist.
   - The fork revision before PF5 (still on KScreenLocker) stays installable for rollback.
2. **Maintaining a fork.** Every upstream merge meets renamed strings and QindaQt commits.
   Mitigations:
   - Keep upstream history.
   - Keep the rename as a checked-in, idempotent script that is re-run after each merge.
   - Keep QindaQt commits small and labelled.
   - Stay on 6.6 until a move is planned.
3. **An identity string missed by the rename**, so the fork reads a stock path or a KDE config file.
   Mitigations: the install-collision check, the sandboxed-`$HOME` config check, and a grep gate
   for leftover `kwin` tokens in the installed tree.
4. **Shared libraries drifting.** A future stock kwayland or layer-shell-qt could stop building
   with the fork. Mitigations: range dependencies with subslot rebuilds, and the trigger rule in §1
   (fork that library then).
5. **Laptop power safety.** A bug could leave a lid-closed laptop running in a bag, or let the
   battery hit zero. Mitigations:
   - The lid inhibitor exists only while the policy runs, so logind's default returns on a crash.
   - The critical action is tested against a fake UPower.
   - The live check runs on battery.
6. **Shortcut compatibility.** Re-creating kglobalacceld's behavior (modifier-only shortcuts,
   releases, keyboard layouts) is the riskiest slice set, and if the registry breaks, every key
   binding breaks with it. Mitigations: do it last, behind the compatibility tests.
7. **Portal breadth.** Browsers, Flatpaks and OBS use every corner of these families. Mitigations:
   flip one routing row at a time, keep the KDE fallback until PF21, and rely on the existing
   real-frontend integration tests.
8. **Profile-parent churn.** Changing the parent rebuilds much of @world. Mitigation: do it once, in
   PF14, from a reviewed pretend diff.
9. **Size.** 60 slices is a large program. Each milestone ships on its own and is useful without
   the next one.

## 8. Open questions for the owner

**Answered:**
1. **Hide the Plasma entries early?** No. They stay until Plasma is actually gone (M2).
2. **Keep Plasma on qinda as a fallback?** No. The architecture is a co-installable fork (§2), and
   qinda's `@kde-desktop` set goes at M2.

**Open:**

3. **Sloom menu add-ons.** They only work inside a Plasma panel. Can they leave the QindaQt package
   set and your world file? Should QindaQt's own global menu learn Sloom Studio's native-Wayland
   menu route (one extra slice)?
4. **KAuth without polkit.** Is that acceptable? The only loss is KWin's helper for killing a hung
   window owned by root.
5. **Shared client libraries.** OK to keep sharing kwayland and layer-shell-qt, as stable client
   libraries, rather than forking them? kdecoration is forked, because the compositor links its
   private part.
6. **Tiles editor.** KWin effects that need Plasma's QML are deleted or ported, and QindaQt's gather
   overview already replaces the overview grid and present-windows. Do you use KWin's tiles editor
   (Meta+T)?
7. **Wayland login screen.** Should the login screen run on the fork (Wayland) instead of silently
   falling back to X11 at every boot (G1)?
8. **How far to go on shortcuts.** Full replacement (M6, 7 slices, highest compatibility risk), or
   keep KDE's small shortcut library embedded in the fork, which runs no Plasma service? The plan
   assumes the full replacement, done last.
9. **The fork's name.** `qindaqt-kwin` is a working name for the package, program, library and
   folders. Do you want a brand of its own before F2 bakes the name in?

## Guiding principle (owner, 2026-09-28)

The goal is not only to remove Plasma parts and dependencies. Each replacement is a **native
QindaQt implementation designed to integrate cleanly with QindaQt**, and QindaQt is designed to
work well with it. It has a QindaTK interface, keeps its preferences in Settings1 and shows them in
QindaQt Settings, and follows QindaQt's appearance tokens and themes. It uses QindaQt's
notifications, shortcuts and shell integration, and exposes QindaQt D-Bus contracts recorded in
ADRs. A like-for-like port of the KDE component does not meet the bar; every slice's acceptance
names its QindaQt integration points.

## Owner decisions (2026-09-28)

These answer the open questions and are binding for the slices above.

1. Plasma's login entries stay until Plasma is actually gone (no early hiding).
2. No Plasma fallback on either host. The goal is to replace KDE and Plasma completely. The
   compositor is a co-installable KWin fork that leaves stock KWin and Plasma installable beside it.
3. The Sloom menu add-ons (`sloom-globalmenu`, `sloom-panelmenu`) leave the QindaQt package set.
4. KWin's KAuth/polkit "kill a root-owned hung window" helper is **replaced with a native QindaQt
   implementation**, not dropped (add a slice to the fork milestone).
5. `kwayland` and `layer-shell-qt` stay shared.
6. KWin's tiles editor (Meta+T) is not used; it goes with the Plasma-dependent effects.
7. The login screen runs on the fork (Wayland) if it can (G1 is in scope, conditional on it working).
8. Global shortcuts are **replaced completely** (M6 in full; KDE's shortcut library is not kept).
9. The fork's name is `qindaqt-kwin` (package `gui-wm/qindaqt-kwin`).
