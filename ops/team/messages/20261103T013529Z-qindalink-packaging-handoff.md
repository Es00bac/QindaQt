# QindaLink packaging lane handoff

Branch link/packaging (base 42edc40cb). Commit: see final line of the lane report / `git log -1` of this branch.

## Changed paths
- packaging/gentoo/gui-wm/qindaqt-desktop/qindaqt-desktop-0.1.0_pre20261002-r49.ebuild (new; template r48, placeholder pin, PATCHES removed, ffmpeg:=[vaapi], android-tools, LINK_APP/LINK_MIRROR ON)
- src/services/link_service/data/qindaqt-link-service.desktop + install rule in src/services/link_service/CMakeLists.txt (login start)
- tests/link/tst_link_login_start.cpp, tests/link/CMakeLists.txt (row qindaqt.link-login-start)
- docs: development/gentoo-desktop.md, architecture/qindalink.md, architecture/session-autostart.md

## Evidence
- Configure with the r49 ebuild's cmake options (Release, prefix /usr, BUILD_TESTING=ON added): exit 0.
- `ninja -C build/pkg qindaqt-link-service qindaqt-link qindaqt-link-mirror qindaqt_link_login_start_test`: exit 0 (419 steps, no -j override).
- `ctest -R qindaqt.link-login-start`: 1/1 passed (entry eligible in QindaQt, ineligible in KDE; Exec uses activation not the binary; unit WantedBy=default.target, no graphical-session).
- DESTDIR install of components QindaQtLink, QindaLinkApp, LinkMirror: 56 files, 6.7 MB. Binaries in /usr/bin; org.qindaqt.Link1.service Exec=/usr/bin/qindaqt-link-service, SystemdService set; unit ExecStart=/usr/bin/qindaqt-link-service; /etc/xdg/autostart/qindaqt-link-service.desktop; org.qindaqt.Link.desktop and icon present.
- RUNPATH of qindaqt-link: $ORIGIN/../lib64/qt6/qml/QindaQt/AppShell; libqindaqt_app_shell.so resolves once the AppShell component is installed at /usr/lib64/qt6/qml/QindaQt/AppShell (verified by placing the lib there; stage alone shows "not found" only because the AppShell component was not staged). service and mirror RUNPATH $ORIGIN:$ORIGIN/../lib64; mirror resolves libavcodec.so.62.
- Caller policy trusted paths = KDE_INSTALL_FULL_BINDIR/qindaqt-link and /qindaqt-shell; qindaqt-shell installs to CMAKE_INSTALL_BINDIR (ShellAppletRuntimeInstall.cmake), so both equal /usr/bin.
- Not run: the full-ebuild compile, a bus-activation live smoke, install.

## Caveats
- QindaQtLink component also installs link_* static libs and headers (/usr/lib64, /usr/include); harmless, left as is.
- r40/42/46/48 are in main (patches dropped); r45 is carried as a patch because its fix is only on fix/keyring-activation-ownership-20261030 (a40cb9f60). Review found the earlier claim wrong; `PATCHES=( r45 )` is restored.
- Forward check of the r45 patch (from QindaGentoo files/) against a scratch export of this branch head: `git apply --check` exit 0 and `patch -p1 --dry-run` exit 0 (CMakeLists hunk at offset 1 line; adds data/qindaqt-keyring-activate, edits org.freedesktop.secrets.service.in).
- Avahi dependency deferred until the LAN lane lands.
- Peer.Ping activation is standard bus behavior but was not exercised live here.

## Cut steps for the PM
1. cut-package.sh excludes the revision being cut when picking its template (newest ebuild in packaging/gentoo, by sort -V). Cutting `pre20261002-r49` itself would template from r31 and lose everything. So run `.cache/small-team/cut-package.sh <full-sha-of-merged-main> pre20261002-r50`: it templates from this r49 file, pins the sha, copies to the overlay and builds r50. (Or, to keep the name r49, copy the r49 file over r31's position manually and sed the pin; r49 does not exist in the overlay.)
2. The commit must contain this branch merged, and must still accept r45 forward: re-run `git apply --check` of the r45 patch against the exact pin at cut time. Do not merge the keyring branch.
3. Keyword line: the script writes /etc/portage/package.accept_keywords/qindaqt-r50 with `=gui-wm/qindaqt-desktop-0.1.0_pre20261002-r50 ~amd64`. Installing pulls ffmpeg[vaapi] and android-tools; check `equery uses media-video/ffmpeg` shows +vaapi (it does on this machine today).
4. After install, log out/in: the autostart entry pings Link1 and the unit starts; or call `busctl --user` Ping to activate immediately.

## Stage manifest
- /etc/xdg/autostart/qindaqt-link-service.desktop
- /usr/bin/qindaqt-link
- /usr/bin/qindaqt-link-mirror
- /usr/bin/qindaqt-link-service
- /usr/lib/systemd/user/qindaqt-link-service.service
- /usr/share/applications/org.qindaqt.Link.desktop
- /usr/share/dbus-1/interfaces/org.qindaqt.Link1.xml
- /usr/share/dbus-1/services/org.qindaqt.Link1.service
- /usr/share/icons/hicolor/scalable/apps/org.qindaqt.Link.svg
- (plus 28 headers under /usr/include/qindaqt/services/link_* and 7 static libs /usr/lib64/libqindaqt_link_*.a)
