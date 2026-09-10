# Installing the full desktop on Gentoo

`gui-wm/qindaqt-desktop` is the Portage candidate for the complete QindaQt
desktop. It builds the native KWin plugin, KDecoration, production shell,
session launcher, services, bundled applications, desktop entries, and shared
QML runtime plugins from one immutable source commit.

The current dated package checkpoint is `0.1.0_pre20260909-r3`, pinned to Git
commit `0eda78efe5fafd4b7b8b592d2fa9d08d398015c5`. Regenerate the package
Manifest whenever this immutable pin changes. This September 9 checkpoint is a
local reviewed snapshot and has not been pushed to the public remote. Its ebuild
uses `RESTRICT=fetch`. Generate the exact source archive locally, then place it
in Portage's DISTDIR:

```sh
qq_source_commit=0eda78efe5fafd4b7b8b592d2fa9d08d398015c5
git archive --format=tar --prefix="QindaQt-${qq_source_commit}/" "${qq_source_commit}" |
    gzip -n > qindaqt-desktop-0.1.0_pre20260909-r3.tar.gz
```

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
ebuild /path/to/qindaqt-desktop-0.1.0_pre20260909.ebuild manifest
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
