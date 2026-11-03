# QindaLink packaging-2 handoff (add-on rescope)

Branch link/packaging-2 (main 6aae5bb1f merged). Nothing installed; no overlay/hub writes; the three Link programs were built once with ccache launchers in one dir (build/p3), now deleted.

## Delivered
- `gui-apps/qindalink-0.1.0_pre20261102.ebuild` template: RDEPEND `>=gui-wm/qindaqt-desktop-0.1.0_pre20261002-r47`, `-DBUILD_TESTING=OFF`, builds only the three Link targets, installs components QindaQtLink/QindaLinkApp/LinkMirror, versions independently of the desktop.
- Desktop template restored as `qindaqt-desktop-0.1.0_pre20261002-r49.ebuild` (placeholder pin, r45 PATCHES, LINK_SERVICE/APP/MIRROR OFF); pre20261102 desktop template and the "_preYYYYMMDD" rule removed from the docs. No desktop cut anywhere.
- `QINDAQT_BUILD_LINK_SERVICE` CMake gate (desktop installs no Link file; verified at configure by inspecting cmake_install.cmake files).
- `~/work_SPaC3/container-wm/.cache/small-team/install-qindalink.sh [--dry-run] [--version V] <commit>` (old install-qindalink-desktop.sh deleted). `set -euo pipefail`; run/run_sh die on failure and run_sh uses `bash -o pipefail -c`; refuses unless the overlay is on master, has nothing staged, and is not behind origin (read-only `ls-remote` check); commits ONLY gui-apps/qindalink (`git commit -- path`, plus a staged-outside-path guard); pushes to the local hub and GitHub; sync, chown, qindalink keyword line, `emerge --oneshot =gui-apps/qindalink-<v>`; no ccache.conf, no MAKEOPTS override, no desktop.
- Docs: gentoo-desktop.md and architecture/qindalink.md (add-on statement; features needing a newer desktop: panel chip, phone input, vault typing).

## Runtime proof against the INSTALLED r47 desktop (qindatk r19), no build-tree paths
- Built from main in build/p3; staged with `cmake --install --component QindaQtLink|QindaLinkApp|LinkMirror`.
- `qindaqt-link` RUNPATH `$ORIGIN/../lib64/qt6/qml/QindaQt/AppShell` resolves to the installed `/usr/lib64/qt6/qml/QindaQt/AppShell/libqindaqt_app_shell.so` (ldd with only that dir; nothing from the worktree).
- `LD_BIND_NOW=1 QT_QPA_PLATFORM=offscreen qindaqt-link --fake populated --grab g.png --grab-after 1500`: exit 0, "wrote ... (1100x720)", no stderr warnings: no undefined symbol, no ABI break, no QML import failure against installed r47 AppShell and QindaTK r19. No fix needed: no private AppShell copy required.
- `qindaqt-link-service` and `qindaqt-link-mirror`: `ldd` shows 0 "not found", nothing from the worktree; both print usage with LD_BIND_NOW=1. The service under `dbus-run-session` with `--no-adb --no-input --no-notifications --port 0` and temp dirs acquired `org.qindaqt.Link1`.
- r47 runtime needs: org.freedesktop.Notifications is owned by qindaqt-notification host on the live session. r47's source (2e00b187e) has the session autostart runner and catalog (session_autostart_runner.cpp, autostart_catalog.cpp; main.cpp scans `ScanOptions::fromEnvironment()` unless --no-autostart), so the QindaQt-only /etc/xdg/autostart entry is honoured, like the installed qindaqt-obs-login.desktop.
- Expected on r47: input and vault typing report unverified (no Session1 / focus relay); the panel chip needs a newer desktop. Service, messages, notifications, app, mirror work.
- Not run: emerge/ebuild, a real phone session.

## Dry run
`PACKAGING_ROOT=<worktree> install-qindalink.sh --dry-run <sha>` exit 0; output printed in the lane's final message.

## Next action
Merge link/packaging-2 to main, then the user runs `install-qindalink.sh --dry-run <main-sha>` and then for real. Desktop reconciliation with the r47 source (51 commits) stays deferred.
