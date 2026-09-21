# Installing the full desktop on Gentoo

`gui-wm/qindaqt-desktop` is the Portage candidate for the complete QindaQt
desktop. It builds the native KWin plugin, KDecoration, production shell,
session launcher, services, bundled applications, desktop entries, and shared
QML runtime plugins from one immutable source commit.

The current checkpoint is `0.1.0_pre20260920`, pinned to
`4fd1372e897ae34d33f11155d4a30a1d7ef3526f`. Regenerate the package Manifest
whenever the immutable source pin changes. The ebuild uses `RESTRICT=fetch`;
generate the exact source archive locally from the pinned commit, then place
it in Portage's DISTDIR:

```sh
qq_source_commit=4fd1372e897ae34d33f11155d4a30a1d7ef3526f
git archive --format=tar --prefix="QindaQt-${qq_source_commit}/" "${qq_source_commit}" |
    gzip -n > qindaqt-desktop-0.1.0_pre20260920.tar.gz
```

The September 20 checkpoint carries five changes and **drops one binary**:

- Panel popups are placed by one owner (ADR-0221). The start panel, the
  launcher browser and the per-service popups opened at their control's
  top-right corner on Wayland because only the desktop controls had the
  edge-aware placement; `QindaQt.Controls.PanelPopup` now owns it for all of
  them.
- The start panel builds only the rows it can show: 16 of 336 on a normal
  install instead of all 336, each of which was resolving an icon through the
  theme inside `popup.open()`.
- `qindaqt-terminal` is **gone** (ADR-0222). QQ_Term (`gui-apps/qqterm`) is
  the desktop's terminal and is a post-dependency of this package, so a
  desktop install always has one; `Terminal=true` desktop entries now launch
  through `qqterm -e`, which never worked before.
- Nine stock layout profiles, one per distinct feel, each with its own default
  theme (ADR-0223); `windows-classic` and `mate-inspired` are retired. A
  `panels.layoutProfile` setting naming a retired profile falls back exactly
  as it does for any unknown id.
- The three applets stock profiles asked for but never had now exist, and the
  start-menu applet takes a `variant` choosing which Windows start menu it
  reproduces (ADR-0224).

Because a binary was removed, this update is **not** a shell-child-only
adoption like r8-to-r9: reinstalling replaces the `Terminal` component's
files, and `gui-apps/qqterm` must be present for the desktop to have a
terminal at all.

The September 19 r9 repair backports the owned-registration lifetime fix to
installed r8 (`c99d39c4`). A disappearing application-menu connection used to
leave the registrar reading a removed hash entry, corrupting the shell heap.
See [the registrar lifetime contract](../shell/global-menu.md#appmenu-registrar-transport)
and [its sanitizer regression](testing-harness.md#current-global-menu-production-shell-proof).
The patch preserves the existing runtime ABI and session contract. For this
specific r8-to-r9 update, the existing supervisor can adopt the installed
repair by replacing only its shell child; KWin, the supervisor, notification
host, and applications remain running.

On qinda, the signed r9 package passed isolated production-shell checks at
1080p, WUXGA, and 1440p before the binary-only Portage merge. The replacement
shell then retired 256 menu registrations across 64 private-peer disconnects
without restarting. All 20 tracked application/session process identities
survived. The separate keyboard/file-browsing source audit remains a later
main-branch change; r9 deliberately carries only this repair over r8. qinda-top
has not been changed by this deployment.

The September 13 r3 checkpoint packages the reviewed delivery through
`33504d61`: current Desktop/Customize behavior, Dock magnification and
attention, Active Application and Global Menu anchoring, external-display
brightness, and authenticated File Manager browsing plus remote open, rename,
new-folder, copy, move, and KIOFuse write-back. It also exposes installed KWin
decoration choices, projects the primary output truthfully in Customize, and
prevents a zero-sized desktop context-menu popup from disconnecting the shell
on Wayland. The package declares
`kde-misc/kio-fuse` directly because remote write-back composition invokes its
session D-Bus service. Installation does not replace a running compositor or
applications; compositor/plugin adoption still requires a later login.

On the September 13 r3 deployment host, the exact plan upgraded only
`gui-wm/qindaqt-desktop` from r2 to r3. Portage inherited
`MAKEOPTS=-j24 -l24`, built and signed the 31,877,120-byte binary package,
merged it with `--usepkgonly`, and verified all 1,337 installed files. A fresh
r2 package remains available for rollback. The supervised shell child and
Settings application were replaced without restarting KWin or the session;
both now map the installed r3 files.

The `-r11` revision lands the desktop gap wave: lock after waking, unlock
grace, PowerDevil lid and power-button actions, and a single Meta+L owner
(ADR-0132); explicit routing for every portal family (ADR-0133); the Input
route for pointers, keyboards, layouts, and global shortcuts (ADR-0134);
gnome-keyring as the Secret Service provider (ADR-0135); and night light in
Settings → Display (ADR-0136). It adds `app-crypt/gcr:4`,
`gnome-base/gnome-keyring`, `~kde-plasma/knighttime-6.6.6`, and
`x11-misc/xkeyboard-config` to RDEPEND. The compositor and decoration plugin
are unchanged, so restarting `qindaqt-shell`, `qindaqt-settings`, and the user
`xdg-desktop-portal` service adopts the revision without a new login.

The `-r10` revision lands the window and container chrome preferences
(ADR-0129), the contained-window handlebar with mouse-wheel roll-up
(ADR-0131), the Luna taskbar repair (ADR-0124 amendment), and window-attached
menus for layouts without a global menu (ADR-0130), whose Active Application
popups now open against their widget and show desktop-entry names. Restarting
`qindaqt-shell`, the settings service, and `qindaqt-settings` adopts the shell
and Settings changes; the decoration plugin and compositor chrome (handlebars,
wheel roll-up, button arrangement) take effect at the next login.

The `-r9` revision adds the three preference icons the Settings sidebar and
Appearance tab strip name (accessibility, wallpaper, font) to the QindaQt
icon theme; only data changes, so restarting `qindaqt-settings` adopts it.

The `-r8` revision lands the Appearance overhaul (ADR-0127) and the
Accessibility route (ADR-0128): the Themes tab shows a preview window whose
title bar, frame, and buttons are painted by the same decoration painter the
compositor uses and whose client area is the real Fusion style with the
previewed palette; the destinations are a glyph-first tab strip with
tooltips; the sidebar shows route glyphs; and Settings gains an
Accessibility route over the consumed accessibility keys. Restart
`qindaqt-settings` and refresh the shell after merging. The decoration
plugin now paints through the shared painter with unchanged output; it
loads on the next session login.

The `-r7` revision links the `QindaQt.Shell.DesktopSurface` static plugin
into the production shell. `-r6` staged the module's files, but Qt links a
static QML module's plugin only when an `import` of it appears in the
target's QML files, and the desktop surface is reached solely from C++, so
the installed shell still reported the module missing. A shell-only refresh
adopts the fix; the `qinda-bliss` profile then hosts desktop icons.

The `-r6` revision installs the `QindaQt.Shell.DesktopSurface` QML module
that `-r5` compiled but never staged. Without it the installed shell logged
`No module named "QindaQt.Shell.DesktopSurface" found` and kept plain
wallpaper when the `qinda-bliss` profile was adopted, so desktop icons never
appeared. A shell-only refresh adopts the module; nothing else changes.

The `-r5` revision ships two things. First, the QindaQt Bliss Luna option set
(ADR-0124, ADR-0125): the `qinda-bliss` theme and wallpaper, the
`qinda-bliss` layout profile with its Luna taskbar and start menu, and the
desktop-icons applet on the new per-output desktop surface. Both are opt-in:
pick the theme and wallpaper on the Appearance page and the layout on the
Customize page; the default `qindaqt` profile and `qinda-dark` theme are
byte-identical to `-r4`. Second, Settings1 no longer refuses to start over a
user-overrides entry its schema cannot normalize (ADR-0126); it ignores and
logs the entry instead, so a key written by another build sharing
`~/.config/qindaqt` can no longer take the shell, dock, wallpaper, and
Appearance route down together. After merging, restart the resident
`qindaqt-settings-service` process (it reactivates on demand) and refresh the
shell; the Settings app adopts the new files on its next start. The Luna
window chrome lives in the KDecoration and KWin plugins, so titlebars take
the Bliss look on the next session login, not through a shell-only refresh.

The `-r4` revision brings Audio1 schema version 2 (ADR-0123): per-channel
volumes and channel maps on devices and streams, and managed virtual
sinks/sources created and removed from the Settings Audio page — the first
Voicemeeter-class slice on PipeWire primitives. It also adds the Appearance
tab's Applications-and-toolkits card, which renders the exact QPalette
ordinary Qt applications receive for the previewed theme (ADR-0115). The
audio service binary changes, so restart `qindaqt-audio-service` (or the
session) after merging; the Settings app and shell adopt the new files on
their next start.

The `-r3` revision makes Apply in the Customize route change the running
desktop (ADR-0122): the shell adopts a saved layout selection or an edited
user-store copy through its existing incremental surface reconciliation —
no shell restart, no compositor restart, no application disruption — with
the startup precedence rules (explicit `--profile` outranks the saved
selection; an unknown id fails closed). The Customize status text now says
the running desktop follows applied changes.

The `-r2` revision redesigns the Settings Customize route into a visual layout
editor (ADR-0121): a layout gallery of miniature desktops, a WYSIWYG monitor
that reproduces the live panel materials from the active theme tokens, an
icon palette, and an icon-first inspector with real applet glyphs through
the confined public iconography module. The change is confined to the
Settings application, its QML modules, the route's tests, and documentation;
no compositor, KWin plugin, or shell-runtime code changed, so the new
Customize page is visible the next time `qindaqt-settings` starts, and a
live shell refresh adopts the reinstalled files without touching KWin or
running applications.

The September 10 `-r1` checkpoint before it pins the dock-experience work:
panel translucency and blur (ADR-0120), panel quick configuration, and the
dock interaction upgrades.

The `-r3` revision vetoes native interactive resize for container members and
removes their resize-only decoration borders (ADR-0117): a grouped member's
frame now changes only through tile-border operations and container reflow.
Because this code runs inside `kwin_wayland`, the fix takes effect on the next
session login after installation, not through a shell-only refresh.

The `-r2` revision makes the panel Bluetooth applet self-sufficient: pairing
initiation with prompt-lane cancel, in-popup PIN/passkey entry, trust toggles,
and device removal ride the existing Bluetooth2 client surface, all rendered
with QindaQt.Controls on the Tokens theme; BlueZ remains the pairing/trust
authority per ADR-0037. Installed over the live session: only the old shell
PID received `SIGTERM`, and the session supervisor relaunched the installed
shell while KWin and existing applications kept their identities. A live
capture showed the applet popup discovering real devices with the new Pair,
Trust, and Forget controls. The first `-r2` merge still applied the calendar
skip patch; after the calendar lane dropped it from the ebuild, the same
pinned commit was re-merged to the complete tree and `qcheck` reports all
1,252 installed files intact, including `qindaqt-calendar`.

The `-r1` revision carries the container drop-targeting fix: user-minimized
containers stay minimized across unrelated scene mutations, a committed drop
activates the moved window instead of the source container, late-Shift
takeover adopts the genuinely moving window rather than the pointer hit, and
every member of a dragged tab page is excluded from drop hit-testing.

Both revisions originally applied a prepare-time patch that skipped the
in-flight calendar app, whose sources did not compile at their pinned commit.
Once the calendar lane landed a compiling candidate on stock Qt Quick
Controls (ADR-0116), the patch was dropped from both ebuilds and the patch
files removed; the complete-tree package now builds `qindaqt-calendar` and
declares `kde-frameworks/kcalendarcore:6`. Both ebuilds therefore require a
source commit that contains the compiling calendar app.

Portage verifies the maintained Manifest before unpacking. The ebuild installs
the complete image through Portage; do not copy executables into the installed
runtime by hand.

The September 9 revision extends the bounded shell icon-theme inventory to
cover the 649 directories declared by the host's standard `hicolor` index.
Application icons stored in later `48x48`, `128x128`, `512x512`, or
`scalable/apps` directories therefore resolve in the dock instead of falling
back to letter tiles. Portage verified and installed the signed binary package;
`qcheck` reports all 1,250 installed files intact. The earlier signed September
8 package remains available for rollback with the unchanged KWin ABI.

The user explicitly requested installation while the desktop and applications
remained running. KWin PID 1390 and session supervisor PID 1478 retained their
original identities and start times. Only the old shell PID 1490 received
`SIGTERM`; the supervisor launched installed shell PID 85067. A live desktop
capture then showed application artwork in every affected dock entry and no
remaining letter tiles. Existing applications were not restarted, and this
installation does not claim a fresh desktop login.

The build used 24 jobs and the site's existing signing subkey. Its first package
attempt completed compilation but produced an unsigned gpkg, which the host's
`binpkg-request-signature` policy correctly rejected. Packaging resumed over the
preserved build with Portage's documented signing variables and the existing
protected passphrase file. The resulting gpkg passed signature verification and
the binary-only merge without changing trust policy. Existing Portage QA notices
for `/usr/Tokens` and `/usr/bin/agent_input` also occur in the previous installed
package; their placement remains a separate packaging cleanup.

The shell-only refresh adopts the fixed task list without reloading KWin
plugins, existing application code, or inherited session environment. Those
boundaries still require application restart or a later login; never restart the
compositor to refresh panels while applications must remain connected.

The package requires the exact KWin 6.6.6 stack and Qt 6.11 or newer. Its direct
runtime closure follows the production process contracts:

| Desktop function | Direct package authority |
| --- | --- |
| Lock screen and compositor shortcuts | release-matched KScreenLocker and KWin `lock,shortcuts` |
| Appearance and other standard portals | `xdg-desktop-portal` plus the release-matched KDE backend |
| Bluetooth | BlueZ |
| Battery and performance profiles | UPower and one standard Power Profiles provider (`tuned[ppd]` or `power-profiles-daemon`); systemd supplies logind |
| Power management, brightness, and idle inhibition | release-matched PowerDevil and KConfig for its persisted idle preferences |
| Audio and network | WirePlumber and NetworkManager |
| Desktop-entry launch | KIO and KService |
| Media keys, idle display policy, screenshots, and authorization prompts | KGlobalAccel, KIdleTime, release-matched KWayland, Spectacle, and the KDE polkit agent |

SDDM remains an operator-selected login manager and is not a package
dependency. Portage must solve the plan without slot conflicts before the
package is considered buildable.

The Power Profiles provider is deliberately an alternative dependency. Gentoo's
`sys-apps/tuned[ppd]` installs `tuned-ppd`, a drop-in provider for
`sys-power/power-profiles-daemon`; it exports the modern
`org.freedesktop.UPower.PowerProfiles` API and the legacy
`net.hadess.PowerProfiles` API that QindaQt consumes. The ebuild therefore uses
`|| ( sys-apps/tuned[ppd] sys-power/power-profiles-daemon )`, so a host with
`tuned[ppd]` keeps its existing provider instead of scheduling a replacement.
QindaQt talks to the public D-Bus contract and does not start or configure
either provider.

The binary native CI job is a compile-and-boot qualification lane rather than
a substitute for this full package plan. It uses the generic
`desktop/systemd` profile and a KWin build without the `lock` USE flag so that
KScreenLocker's Gentoo `PDEPEND` does not pull Plasma Workspace and Plasma login
sessions into the build image. The package plan below intentionally retains
KWin `lock,shortcuts` and KScreenLocker; validate that larger runtime graph in a
clean Portage image before publication.

Copy `packaging/gentoo/gui-wm/qindaqt-desktop/` into a configured local overlay,
regenerate the Manifest, run the repository's package QA, and inspect the plan:

```sh
ebuild /path/to/qindaqt-desktop-0.1.0_pre20260913.ebuild manifest
pkgcheck scan --repo your-overlay gui-wm/qindaqt-desktop
emerge --pretend --verbose gui-wm/qindaqt-desktop
```

Users with `gui-apps/qindaqt-apps` installed must first remove its explicit
selection, without unmerging its files:

```sh
emerge --deselect gui-apps/qindaqt-apps
emerge --pretend --verbose gui-wm/qindaqt-desktop
```

The weak blocker can then put the full package before removal of the old package
record, transferring the overlapping application files without an unowned gap.
If the old package remains in `@selected`, Portage correctly reports an
unsatisfied blocker because it has been asked to retain both owners. Review the
plan and keep a binary package of the old state; do not force file collisions or
unmerge application files by hand.

Build a reusable package with the site's normal signing policy:

```sh
emerge --buildpkgonly gui-wm/qindaqt-desktop
```

Run the [release gates](releases.md) against the exact source before merging.
Then end the running QindaQt session and install from a text console or another
desktop:

```sh
emerge --ask --usepkg gui-wm/qindaqt-desktop
```

If your user session stays running in a text console or uses systemd lingering,
reboot after the merge. Logging out of the desktop alone can leave older D-Bus
services running, even though Portage has replaced their executables and schemas.

After a fresh login, confirm the KWin plugin service, production panels, session
services, and all three application desktop entries. Inspect
`/var/db/pkg/gui-wm/qindaqt-desktop-*/CONTENTS` to confirm Portage owns their
executables and QML/plugin files. If the installed smoke fails, leave the
session and restore the retained prior compositor stack and QindaQt binary
package together. The [KWin upgrade procedure](kwin-upgrades.md) explains the
ABI-safe sequence.

For application-only installation, keep using
[`gui-apps/qindaqt-apps`](gentoo-apps.md); the two packages are alternative file
owners and are not installed together.
