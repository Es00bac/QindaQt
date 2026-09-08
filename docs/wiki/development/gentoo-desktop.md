# Installing the full desktop on Gentoo

`gui-wm/qindaqt-desktop` is the Portage candidate for the complete QindaQt
desktop. It builds the native KWin plugin, KDecoration, production shell,
session launcher, services, bundled applications, desktop entries, and shared
QML runtime plugins from one immutable source commit.

The current dated package checkpoint is `0.1.0_pre20260908`, pinned to Git
commit `c1952e90a84005aa3ce3aee07cea1b6d89b6a35c`. Regenerate the package
Manifest whenever this immutable pin changes. This September 8 checkpoint is a
local reviewed snapshot and has not been pushed to the public remote. Its ebuild
uses `RESTRICT=fetch`. Generate the exact source archive locally, then place it
in Portage's DISTDIR:

```sh
qq_source_commit=c1952e90a84005aa3ce3aee07cea1b6d89b6a35c
git archive --format=tar --prefix="QindaQt-${qq_source_commit}/" "${qq_source_commit}" |
    gzip -n > qindaqt-desktop-0.1.0_pre20260908.tar.gz
```

Portage verifies the maintained Manifest before unpacking. The ebuild installs
the complete image through Portage; do not copy executables into the installed
runtime by hand.

The September 8 redesign is installed as this package. The user explicitly
requested the merge while existing sessions remained running. Portage verified
and installed the signed binary; all 1,067 payload files match its image and
package ownership. Installed File Manager UI actions and native Editor/Terminal
startup and PTY checks pass. Already running applications use their loaded
version until restarted; this installation does not claim a fresh desktop login.

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
ebuild /path/to/qindaqt-desktop-0.1.0_pre20260908.ebuild manifest
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
