# Office r3 signed build-only started

- Time: 2026-10-12T16:51:08+00:00
- Exact overlay: 9f6eed509cfe027bced4e1b00b4ef1a15d4e35be
- Exact source: d47c4d905534bea0b2d65394eebf35dc5b7bbab5
- Independent recipe reviewer cleared the source-bound recipe for one build; final binary acceptance remains pending.
- Qinda wrapper PID: 2832508, launched at 2026-10-12T16:50:40Z.
- Work area: ~/work_SPaC3/.qa/office-printing-r3-20261012
- Command: sudo -n env PORTAGE_CONFIGROOT=<work-area>/config DISTDIR=<work-area>/distfiles PKGDIR=/var/cache/binpkgs/.qa-office-printing-r3-20261012 emerge --buildpkgonly --oneshot --nodeps --usepkg=n --getbinpkg=n --autounmask=n --autounmask-write=n --ask=n '=gui-apps/qindaoffice-0.1.0_p20261010-r3::qindaqt'
- Exact command, archive digest and configuration observation: logs/build-command.json. Build: logs/office-r3-build.log. Final status will be logs/build-result.json.
- Build-only pretend exit 0: one package, zero downloads. Qt CUPS already installed; printing services are runtime-only, so no service stack merge is required on the build host.
- Preflight: root free22364168192 bytes, binary volume free1808427646976 bytes, available memory27162156kB, load1.17. Configured MAKEOPTS stays -j24 -l24; no job/load override.
- Installed Office remains r2; original world SHA256 recorded. No session or printer actions occur here.
