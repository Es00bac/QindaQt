# Read-only two-host native Portage cutover plan

- Recorded: 2026-10-02T20:57:35Z
- Policy inspected: overlay39321582993a8a1cb3295c92d39517065358a441; now published identically through merge502a04237ed9e36832f1f578a2f3716d5959c367.
- Exact candidate roots: =gui-wm/qindaqt-desktop-0.1.0_pre20261002-r2, =gui-wm/qindaqt-kwin-6.6.6_p1-r3, =app-accessibility/gabbee-0.1.0_p20261002.
- Read-only snapshots: qinda 2026-10-02T20:53:15Z /1.042s; qinda-top 2026-10-02T20:53:15Z /0.898s. Both snapshot commands exit0.
- No install, world/profile/config mutation, unmerge, solver rerun, restart, audio, physical-session, game or GPU action.

## Actual installed state

Both selected profile symlinks already point to qindaqt:qindaqt/systemd, but installed overlay8f23e1e90490ab02f49e88d5c99994bbf1f0082b still gives that profile the Gentoo desktop/plasma/systemd parent and explicit KDE portal/polkit/Sloom menu roots. Accepted502 updates the SAME profile path to ordinary desktop/systemd; no eselect switch is necessary. Verify actual parent after sync, do not infer from the symlink name.

qinda has60 forbidden kde-plasma packages; qinda-top has30. Both additionally have the two permitted kde-plasma/kwayland and kde-plasma/layer-shell-qt. Every exact forbidden CPV, USE/SLOT/repo and active installed reverse edge is bound in the adjacent JSON and host .atoms files. All kde-frameworks packages remain outside this removal list.

Both world files explicitly root kde-plasma/sloom-globalmenu and kde-plasma/sloom-panelmenu. All other forbidden CPVs are not explicit entries in the world file. qinda additionally roots @kde-desktop, whose exact local set contains plasma-meta and spectacle plus seven KDE applications. Retire only its saved set membership; preserve the set file and unrelated @ai-tools/@boot-essentials/@codecs/@extras/@lxqt-desktop.

Installed desktop is qinda pre20260927-r7 / laptop pre20260928-r3; Gabbee is pre20260927 on both. No native gui-wm/qindaqt-kwin is currently installed in either snapshot.

## Dependency decisions needed before zero-installed-Plasma

Active USE-reduced metadata is not a solver-selected OR graph. Claude declares portal KDE/GTK/GNOME alternatives and cli-tools/trash-cli/gvfs alternatives. Qinda already has portal-gtk; laptop has none of the non-KDE alternatives. Neither snapshot has trash-cli or gvfs. LibreOffice's cli-tools alternative is already satisfied by xdg-utils. Preserve these apps: select a valid non-Plasma provider or reviewed native-provider recipe, rather than unmerge the app or assert every declared OR edge is mandatory.

Both: xdg-utils[plasma] actually requires cli-tools; Sloom Studio[globalmenu] requires sloom-globalmenu; KIO6.27 has unconditional keditfiletype PDEPEND; KAuth6.27[policykit] requires polkit-kde-agent; kirigami-addons1.12.1 has unconditional libplasma RDEPEND; kio-extras[activities] requires Activities/stats. Qinda additionally has gwenview[activities], KDEConnect libplasma and LXQt config[monitor] libkscreen. Unrooted xwaylandvideobridge also declares kpipewire; let the reviewed graph determine its disposition, do not force-remove an app.

Existing bounded choices: xdg-utils -plasma, Sloom Studio -globalmenu, kio-extras -activities, qinda gwenview -activities if retaining it, qinda LXQt config -monitor. KIO matching6.27/6.29-r1 accepted recipes already omit only the association-editor post-dependency. These are proposed package USE/rebuild inputs, not changed settings.

KAuth: existing IUSE=+policykit, PDEPEND=policykit? ( kde-plasma/polkit-kde-agent:* ); src_configure uses cmake_use_find_package policykit PolkitQt6-1. Disabling policykit removes privileged backend functionality. Preserve it; a narrowly reviewed native agent provider/PDEPEND revision is needed.

Kirigami: exact upstream1.12.1 official KDE archive SHA b24de84a9ea1ec88f4d4184395d4e6ace5eb27db987f161b92b23c318fd43504 (3,251,461 bytes);223 source/CMake/QML/header files inspected. No Plasma find/link/import;8 token hits are comments/text/sound-theme names. Installed addon QML likewise has no org.kde.plasma import. No optional Plasma component or existing USE flag. Current recipe edge appears overbroad; propose a minimal recipe-only removal with real QML smoke, preserving every module. This is source evidence, not a compiled/runtime acceptance.

KDEConnect26.04.3-r3: unconditional libplasma runtime atom; upstream Linux CMake unconditionally adds plasmoid, whose CMake solely calls kpackage_install_package(... plasmoids plasma SKIP_APPSTREAM). No current plasmoid USE. A narrow optional plasmoid packaging choice can retain daemon/app/indicator/SMS/CLI and omit the Plasma applet. Not implemented or build-qualified.

The independent closure worker reports exact393 two-root PF25 result589qinda/576laptop/0forbidden, private candidate config only. That excludes installed old world/application roots and does not prove all installed Plasma is removable.

## Manager-only staged commands (not executed)

1. Preserve rollback BEFORE syncing/upgrading/removing. Save world, world_sets, profile symlink, relevant /etc/portage/sets files and package.use overrides with permissions in a task-private0700 archive. Keep installed overlay8f23e1e and exact source recipes/Manifest inputs. Produce/verify Portage binary backups for current desktop, Gabbee, all intended forbidden removals and any rebuilt consumers:

   sudo quickpkg --include-config n =gui-wm/qindaqt-desktop-<observed-old-version> =app-accessibility/gabbee-0.1.0_p20260927
   xargs -r -a <this-host-forbidden.atoms> sudo quickpkg --include-config n

   Exact old desktop versions above are host-specific; binary backup/config archive restoration is a manager gate. Current binpkg candidate lists are NOT rollback proof: qinda includes zero-byte old Sloom archives; neither list has its exact installed desktop revision. Check nonzero archive, metadata CPV/USE and signature/hash, not filename alone. Retain modified configuration separately.

2. After manager accepted publication/archive/signatures, sync the accepted overlay via existing Portage workflow (sudo emaint sync -r qindaqt). Check installed repo commit contains502 and profile parent is now gentoo:default/linux/amd64/23.0/desktop/systemd; preserve divergent/local edits instead of resetting them. Profile selection remains qindaqt:qindaqt/systemd.

3. Prepare/install only accepted three native roots through existing qinda-sync:
   qinda-sync packages --prepare-only =gui-wm/qindaqt-desktop-0.1.0_pre20261002-r2 =gui-wm/qindaqt-kwin-6.6.6_p1-r3 =app-accessibility/gabbee-0.1.0_p20261002
   qinda-sync packages =gui-wm/qindaqt-desktop-0.1.0_pre20261002-r2 =gui-wm/qindaqt-kwin-6.6.6_p1-r3 =app-accessibility/gabbee-0.1.0_p20261002

   Preparation itself may refresh overlay/keyword configuration: these are future authorized manager actions, not read-only inspection commands. Retain actual native fresh-login readiness and rollback checkpoints before retirement.

4. Both, only after accepted source/install gates:
   sudo emerge --deselect kde-plasma/sloom-globalmenu kde-plasma/sloom-panelmenu
   qinda only: sudo emerge --deselect @kde-desktop

   No other world root or set membership is removed. This deselects saved roots without unmerging packages. Apply the explicitly reviewed minimal USE/provider recipe changes above. Review ordinary profile migration with emerge --pretend --verbose --update --deep --newuse @world; no global Plasma masks and no indiscriminate rebuilding/removing user apps.

5. Repeat exact installed PF25 two-root guard after native install:
   /var/db/repos/qindaqt/tools/check-plasma-dependencies --installed --root =gui-wm/qindaqt-desktop-0.1.0_pre20261002-r2 --root =gui-wm/qindaqt-kwin-6.6.6_p1-r3

   Refresh the actual forbidden .atoms inventory because the attached lists are timestamped pre-install inputs. Run only scoped pretend:
   xargs -r -a <fresh-this-host-forbidden.atoms> sudo emerge --pretend --verbose --depclean

   Review all retained consumer blockers, full dependency and preserved-library warnings. Once the exact removal list is accepted and rollback/fresh native login is qualified, root may execute the same scoped --depclean command without --pretend. Do not use --unmerge/--nodeps or an unrestricted depclean; do not mask/delete frameworks, KWayland or LayerShellQt. Recheck zero forbidden installed packages and native installed closure afterward.

## Verification boundary / next action

Two actual VDB/world/profile metadata inspections passed exit0; source CMake/recipe probes passed. Full snapshot hashes and source archive receipt are in adjacent JSON/ignored build/portage-cutover-plan. No resolver, build, install, unmerge or physical session test was performed by this worker. Current external mandatory/provider decisions prevent a truthful unconditional zero-installed-Plasma command.

Requested manager action: review source evidence and route only the minimal provider/optional packaging choices; then execute accepted install/profile/world/scoped depclean gates. Available to implement a narrowly assigned recipe candidate in an isolated overlay worktree. Source toolchain/app behavior acceptance remains separate.
