# Actual matching-version Portage/provider strategy

- Recorded: 2026-10-02T21:07:13Z
- Source candidate:851e2c0f6ae434128db517946b5f391101d3b674; independent Input reviewer owns acceptance.
- Exact actual qinda private pretend exit0/9.749801737s;9 packages,4 revision upgrades/5 reinstalls, no KF version upgrades.
- Log SHA256:ed9b98e3515c49cea93eb4c7a5e759b647649e0639a74e94dc4b8e9eee996da7.
- Own candidate unchanged; no compiler/install/unmerge/world or host configuration action.

Use accepted overlay/native-profile after manager integration; the actual probe uses task-private PORTAGE_CONFIGROOT, PORTAGE_REPOSITORIES pointing851 and task cache. Preserve installed coherent KF6.27 using explicit exact revisions and --dynamic-deps=n, without --update/--deep/--emptytree or @world. Exact successful argv:

    emerge --ignore-default-opts --pretend --verbose --oneshot --newuse --with-bdeps=n --dynamic-deps=n --autounmask=n --autounmask-write=n =kde-frameworks/kauth-6.27.0-r1 =dev-libs/kirigami-addons-1.12.1-r1 =kde-misc/kdeconnect-26.04.3-r4 =kde-frameworks/kio-6.27.0-r1 =x11-misc/xdg-utils-1.2.1-r11 =media-gfx/sloom-studio-bin-0.9.16 =lxqt-base/lxqt-config-2.4.0 =kde-apps/kio-extras-26.04.3 =kde-apps/gwenview-26.04.3

This schedules xdg-utils -plasma, Sloom -globalmenu, KAuth6.27-r1 policykit retained, LXQt config -monitor/touchpad retained, Kirigami1.12.1-r1, KIO6.27-r1, KDEConnect26.04.3-r4 -plasmoid, kio-extras -activities and gwenview -activities/-semantic-desktop. It does not upgrade200 Frameworks packages. A permitted next **build-only** gate can replace --pretend by --buildpkgonly --buildpkg after different review/resource grant, preserving exact atoms, profile and installed ABI set. Manager reviews actual signing/Manifest and app/QML/agent smoke before installation. Both flags/build source requirements remain actual Portage gates; this successful pretend does not prove compilation. No --binpkg-respect-use=n shortcut: mismatching old binaries cannot supply these changed flags.

Actual installed qinda Claude1.49585.0 direct runtime solver selection exit0/2.307427218s, using read-only load_emerge_config/backtrack_depgraph and mtimedb.make_readonly: kde-plasma/xdg-desktop-portal-kde6.6.6, kde-plasma/kde-cli-tools6.6.6, gnome-keyring48. It chooses KDE even with portal-gtk also installed. Adjacent JSON contains exact options and all selected direct runtime children; no all-OR approximation.

To preserve Claude during cleanup, a reviewed native portal-provider OR option (native Desktop already implements the actual service) and a valid trash-cli/gvfs alternative must be supplied before old KDE providers are retired. Merely listing declared alternatives is not a branch-change proof. Existing allowed GTK provider is an alternative if manager chooses it; laptop currently lacks it. No Claude package recipe/provider decision was changed here. Actual future installed/depclean solver must be rechecked after accepted provider changes/removal; this is the current selected graph only. No laptop solver or physical session action ran.

Requested next action: exact Input source review, then manager-granted matching nine-target Portage build-only/app smoke. Available for a narrowly assigned Claude provider adaptation; no forced app removal, global mask or dependency loosening. Root retains all installs/world/profile/depclean/fresh login authority.
