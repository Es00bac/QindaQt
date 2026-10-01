# Plasma-free packaging audit — 2026-10-01T08:35:00Z

Worker: pf-packaging-sol-20261001. Base: ab7640944d67b49f5fcbd198a96ac6c70cad193c.
Read-only audit; no source/package/service changes or real secrets examined.

## Authoritative pins and preservation

- QindaGentoo qinda checkout master and bare hub master both directly report
  `d2fe9adf6469a396f6417c1864b8a01fd31f2851`. Checkout porcelain is empty.
  Its origin tracking ref is 4 commits behind; the assignment's ahead15 state
  is superseded by these observations. No fetch/reset/push was done there.
- Shared `metadata/qinda-delivery` still names
  `=gui-wm/qindaqt-desktop-0.1.0_pre20260927-r7`; source
  `6a9ca787215dd219d0a9e063a70214a8e6a45375`.
- Installed qinda: that r7 desktop and stock KWin `6.6.6-r1`.
  Installed qinda-top: desktop `0.1.0_pre20260928-r3`, source
  `e86325ce572c01004de7ba4be271d9ffb66bbb45`, stock KWin `6.6.6-r2`.
  Neither host has installed gui-wm/qindaqt-kwin.
- Audit consumer manifest pins fork
  `6ab6c01ede8143a7ddb477d6f0040e9b2f3753e4`, tree
  `99a81290288d4f72742fd85ae4fb21b752ecd224`, CMake `6.6.6.1`,
  package `gui-wm/qindaqt-kwin-6.6.6_p1`.
- Qinda's moving consumer checkout has a newer manifest pin `5f6fdf1124f4e600cc77129be91b09e0d2880b86`;
  moving fork main/hub was `2e4e1961169febaf3e2a1f6d6eeede9510d18ac2` at observation.
  These are observation only, not a qualified release pair. Consolidate exact
  reviewed source before producing immutable final package revisions.

## Preserved overlay candidates are not integrated delivery

- `feature/qindaqt-kwin` / clean worktree `QindaGentoo.worktrees/qindaqt-kwin`:
  `1819e4a9f923d037e6b618038d501f407b9eae07`. Fork p1 recipe pins old
  `0dd2fdb802c6dfdecb4771942b05788a8aa386b5`. It still requires kscreenlocker,
  kglobalacceld, knighttime, plasma-activities, libplasma and milou;
  `KWIN_BUILD_SCREENLOCKER=ON`. It is an M1 recipe, not a final Plasma-free one.
- `feature/qindaqt-kwin-desktop` / clean f5 worktree:
  `20e6238836e55e97daa50a006972995c5fe5b495`. Adds desktop pre20260928-r4
  source `e0f9df545b9dec9a094271176522676f8d135a44` and fork delivery row.
  That desktop still requires kscreenlocker, polkit-kde-agent, Activities,
  powerdevil, knighttime, spectacle, portal-kde and gnome-keyring.
  Its delivery-edit commit title is not evidence of installation.
- Preserved stock-recipe removal branch:
  `b590ad178667586a782be58bc61007891ad8c2d8`.
- GeoClue night-light recipe branch/worktree:
  `e4f437f31e0cbbdb5d45c24ebf95a411f5a295e0`, adding pre20260930 recipe.
  Coordinate consolidation; do not overwrite any preserved recipe/branch.
- Authoritative master contains no fork recipe, PF25 dependency guard or
  `plasma-free` subprofile. Its tools are sync-from-projects, push-to-laptop,
  qinda-sync. Scoped repository searches found no dependency guard in the
  audited consumer tools/packaging or authoritative overlay tools/profiles.

## Actual installed dependency paths

Both installed desktops directly retain nine forbidden final-plan Plasma nodes:
kwin, kdecoration, kscreenlocker, polkit-kde-agent, plasma-activities,
powerdevil, knighttime, spectacle, xdg-desktop-portal-kde. Shared kwayland
and layer-shell-qt are allowed. Both directly retain gnome-base/gnome-keyring.

Installed RDEPEND traversal found 24 distinct forbidden Plasma nodes per host:

- desktop -> stock kwin -> kglobalacceld, aurorae, breeze, libplasma, milou
- desktop -> kscreenlocker -> libkscreen
- desktop -> spectacle -> kpipewire
- desktop -> powerdevil -> plasma-workspace -> plasma-activities-stats,
  libksysguard, kactivitymanagerd, kdesu-gui, plasma-integration
- plasma-workspace -> kdesu-gui -> kde-cli-tools-common
- plasma-workspace -> plasma-integration -> qqc2-breeze-style

The direct nine plus these fifteen explain the 24. Traversal used Portage's
installed vartree dbapi, USE-reduced RDEPEND, installed matches and BFS; qinda
633 reachable nodes, qinda-top 622, zero parse failures, commands exit0.
This is installed runtime reachability, not a new solver plan: all installed
OR alternatives were traversed, PDEPEND/DEPEND/BDEPEND were not included,
and absent matches were skipped. A future PF25 gate must inspect the newly
resolved full dependency graphs rather than reuse this snapshot.

## Profile versus independent owner selections

Both selected profiles resolve to `/var/db/repos/qindaqt/profiles/qindaqt/systemd`.
Authoritative parent remains `gentoo:default/linux/amd64/23.0/desktop/plasma/systemd`.
Its package.use still names kwin lock, kwin-x11 lock, plasma-login-sessions,
and plasma-nm. Its packages still select portal-kde, polkit-kde-agent and
both Sloom menu applets. Both world files explicitly select the Sloom applets;
qinda world_sets additionally selects `@kde-desktop`, laptop world_sets is empty.
Qinda has 62 installed kde-plasma packages versus laptop32 (inventory observations;
counts are descriptive and not depclean predictions). Qinda kwallet-pam is installed;
laptop inventory has no kwallet-pam. GNOME keyring48.0-r1 is installed on both.

QindaQt's closure must pass independently of unrelated selected packages.
Owner decisions authorize no Plasma fallback on either host and removal of
Sloom add-ons from the QindaQt set, while architecture must keep Plasma
co-installable. Remove approved selections only in the reviewed rollout;
do not add global masks or remove unrelated KDE apps solely for package-category
membership. SDDM remains operator selected, not a desktop RDEPEND.

## Co-installation and physical/migration gates

HANDOFF documents historical qualified private stage at exact6ab6c01:453 paths,
zero collisions against1059 stock paths; selected52 manager rows qualified.
This audit did not repeat those gates. Fork checker is
`qindaqt/tools/check-install-collisions`, comparing staged paths with installed
kwin/kwin-x11/kdecoration manifests and checking leftover identities.
F8's installed co-use proof remains open: both hosts still run stock packages;
no audit evidence establishes physical fork login, stock virtual startup beside
it, independent configuration stores, or final installed co-installation.
The historical broad upstream suite has a bounded private KDecoration-header
failure; do not report a full-suite pass.

Approved plan/release sequence:

1. Integrate exact final source/recipe/profile/guard candidates in isolated overlay
   worktrees; preserve released recipes and update README/wiki/ADR consequences.
   New full desktop must set QINDAQT_BUILD_REMOVABLE_MEDIA=OFF while the component
   owns those files. Match compositor/plugin exact version and fork serial.
2. PF25 guard resolves desktop AND fork closures with only kwayland/layer-shell-qt
   allowed at final milestone; negative forbidden-dependency fixtures and optional
   per-machine plasma-free opt-out must not prevent co-installation globally.
   Include profile-parent change to desktop/systemd and reviewed USE/world impact.
3. Run metadata/Manifest/pkgcheck, clean full Portage build, install-file ownership
   and collision checks, exact native build/plugin boots, focused and broad available
   gates, strict MkDocs/link checker, and exact candidate independent review.
4. Build/test qinda first, laptop low-priority systemd-run j6 per plan (manager may
   lower resource cap); retain signed rollback packages/source and inspect pretend
   world/profile update and depclean plans. Shared metadata/qinda-delivery changes
   only after verified recipes/archives are preserved in hub.
5. End desktop and install from text console/other desktop; lingering user processes
   require reboot. Fresh-login smoke checks panels, plugin, residents, portals,
   shortcuts, native lock/PAM, physical input/output/hotplug, battery lid/critical
   safety, and conditional SDDM Wayland fork startup. Keep Plasma entries until
   Plasma actually leaves. Depclean only after qualification and reviewed removals.
6. Key-store retirement requires a separate actual one-time migration: retain original
   stores/backups; independently select provider and accepted session/display lineage;
   quiesce legacy stores; stop native destination writer while sources stay running;
   use owned native prompts/anonymous FD passwords; verify native sealed data and
   real app compatibility before provider name switch and removal of GNOME/KWallet
   autostart/PAM/packages. Original stores remain untouched until owner removal.
   Synthetic importer tests do not qualify real stores or installed helper journeys.

## Handoff and bounded caveats

Source documents read: AGENTS, wiki index, top HANDOFF, plan packaging/owner/risks,
release, Gentoo desktop, install checkpoint, keyring-storage/import, Platform queue;
overlay README/CLAUDE and preserved branch recipes read over explicit qinda SSH.
Git/inventory/Portage traversal queries exit0; absent rg matches are expected
negative discovery. No compilation, native/doc gates, emerge, service action,
package edits, unmerge, depclean or migration executed. No secrets/private records
were read or output. This is a packaging audit, not milestone completion.

Requested next action: manager use these exact gaps to assign consolidated overlay
implementation/review, with explicit path ownership and immutable source pair.
Worker is waiting for explicit root task; offers read-only exact-recipe/PF25 guard
review after candidate exists. Platform queue inspected; no implementation claimed.
