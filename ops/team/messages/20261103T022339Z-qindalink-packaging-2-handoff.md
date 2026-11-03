# QindaLink packaging-2 handoff

Branch link/packaging-2 (base 901dc29b5, merged main 6aae5bb1f incl. link_focus, link_vault, QINDAQT_LINK_TEST_HOOKS). Nothing compiled, nothing installed, no overlay/hub writes.

## Delivered
1. `packaging/gentoo/gui-apps/qindalink/qindalink-0.1.0_pre20261102.ebuild` (+ metadata.xml): builds only qindaqt-link-service, qindaqt-link, qindaqt-link-mirror; installs components QindaQtLink, QindaLinkApp, LinkMirror (service, D-Bus activation file, systemd user unit, autostart entry, desktop entry, icon) and removes build-time static libs/headers. RDEPEND: gui-wm/qindaqt-desktop >=0.1.0_pre20261102 (qindaqt-link's RUNPATH finds libqindaqt_app_shell.so in the desktop QML tree; session bus, portal, notification host), qindatk, ffmpeg[vaapi], android-tools, openssl 3.2, Qt. BUILD_TESTING=OFF (keeps QINDAQT_LINK_TEST_HOOKS out).
2. `QINDAQT_BUILD_LINK_SERVICE` (default ON) gates link_protocol/transport/adb/notifications/messages/focus/input/macros/vault/service and tests/link; `link_client` always builds (applet) but installs only when SERVICE or APP is ON. The desktop ebuild passes SERVICE/APP/MIRROR OFF.
3. `qindaqt-desktop-0.1.0_pre20261102.ebuild` (from r49 content: r45 PATCHES kept, ffmpeg/android-tools removed, Link options OFF). r49 template deleted. Rule documented in docs/wiki/development/gentoo-desktop.md.
4. Archive prefix/S = `QindaQt` in both ebuilds; cut-package.sh edited (prefix only; its MAKEOPTS guard and placeholder-template logic untouched). ccache lines and rationale in the wiki (see below).
5. `~/work_SPaC3/container-wm/.cache/small-team/install-qindalink-desktop.sh [--dry-run] [--snapshot preYYYYMMDD] <commit>`.

## Evidence (configure-only, ccache launchers, one build dir build/p2, deleted)
- Desktop options (SHELL/KWIN/VIEWER ON, LINK_SERVICE/APP/MIRROR OFF, BUILD_TESTING=OFF): configure exit 0. No cmake_install.cmake anywhere references QindaQtLink, QindaLinkApp, LinkMirror, qindaqt-link*, org.qindaqt.Link or libqindaqt_link*; src/services/link_client/cmake_install.cmake has no install rule. So the desktop installs no Link file; the applet still configures.
- qindalink options (SHELL/KWIN/VIEWER OFF, LINK_* ON): configure exit 0; `ninja -t commands` for the three targets = 224 compile steps (apps/link 39, controls 32, shell/global_menu 20, link_mirror 16, link_service 15, design_tokens 11, app_shell 10 ...). All 8 libqindaqt_link_*.a that the QindaQtLink component installs are built by those targets, so the staged install cannot hit a missing file.
- Not run: any compile, `cmake --install`, DESTDIR stage (needs a build), emerge.
- `install-qindalink-desktop.sh --dry-run --snapshot pre20261102 744980555...` (PACKAGING_ROOT pointing at this worktree, since the templates are not on main yet): exit 0, printed every step with no sudo, and the r45 patch check (`git apply --check` + `patch -p1 --dry-run` on a scratch export) passed.

## ccache across snapshots
One-time root lines for /var/cache/ccache/ccache.conf (the script applies them idempotently):
    base_dir = /var/tmp/portage
    hash_dir = false
    sloppiness = file_macro,locale,time_macros
Why it works: cmake.eclass uses BUILD_DIR=${WORKDIR}/<PF>_build, a sibling of S=${WORKDIR}/QindaQt. With base_dir ccache rewrites absolute paths under /var/tmp/portage relative to the cwd, so every command says ../QindaQt/src/..., identical across snapshots; the version only appears in the cwd name, which hash_dir=false keeps out of the hash. Remaining misses: moc `mocs_compilation.cpp`/qrc units include generated files by absolute build path (hashed text contains <PF>); any edited header misses its dependents; compiler/flag changes. No hit rate is claimed: unmeasured (earlier global-conf-only attempt got 12-13%).

## Caveats
- First install is a full desktop compile (new version, cold cache); later snapshots are the benefit.
- r45 must still apply forward at cut time (script enforces). A commit already containing r45 would make the script refuse.
- qindalink is not a dependency of the desktop; the applet shows "service unavailable" until it is installed.
- No ADR written for the package split; the wiki records it.

## Next action
Merge link/packaging-2, then the user runs `install-qindalink-desktop.sh --dry-run <main-sha>` and then without `--dry-run` in their terminal.
