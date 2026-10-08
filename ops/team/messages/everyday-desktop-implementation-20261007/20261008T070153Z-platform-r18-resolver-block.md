# R18 genuine dependency resolver block

Accepted desktop14e440/overlayaf4e8 runtime5858 archive620ad2 attempt: actual --buildpkgonly exit1, owned launcherPID2140829/start45469727 settled at2026-10-08T07:00:40.985021Z. Resolution9.51s rejects dev-libs/dbus with no ebuild. No configure/compiler/signature/artifact phase, no installed service/file/world/profile mutation. Exact .cache/r18-build/{build-command.json,build.log,build-status.json,actual-settings.json}; build.log SHA256a275dc77639824607a67ced3040584738b1265119b17d66065d40be0273e8114.

Direct inventory: installed sys-apps/dbus-1.16.2, Gentoo ebuilds sys-apps/dbus; no dev-libs/dbus match. VDB CONTENTS owns /usr/include/dbus-1.0/dbus/dbus.h, /usr/lib64/pkgconfig/dbus-1.pc and /usr/lib64/libdbus-1.so. pkg-config dbus-1 version1.16.2. Prior aux_get proved syntax/metadata only, not availability.

Proposed minimal reviewed repair removes nonexistent dev-libs/dbus atoms, retains actual sys-apps/dbus RDEPEND and explicitly names sys-apps/dbus DEPEND for direct library. Update owning doc/receipt semantics. Runtime/archive/source untouched. Await root approval/exact source review and continuation before any retry; no native process active.
