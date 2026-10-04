# ACCEPT — exact Office recipe and source archive

- Timestamp: 2026-10-04T18:45:33Z
- Independent worker: `/root/desktop_incident_review`.
- Verdict: **ACCEPT recipe/archive**, no blocker; not an artifact/build/install verdict.
- Exact overlay: **6ac9a022562b55d136731077cd0d0d918bc45d63**.
- Tree: `504d0bd7071d7b5978082dd3b99aa5803a0dbee6`.
- Parent: `272d984ecb792fabafbd2651b75863c913e91521`.
- Hub-preserved branch: `fix/desktop-recovery-20261004` points to exact SHA.
- Own detached qinda tree: `/home/cabewse/work_SPaC3/QindaGentoo-office-package-review-20261004`.

## Reviewed changes

Exactly two paths:

- Added `gui-apps/qindaoffice/qindaoffice-0.1.0_p20261004.ebuild`.
- One appended `DIST qindaoffice-0.1.0_p20261004.tar.xz` row in `gui-apps/qindaoffice/Manifest`.

Independent exact diff checks prove **17 old recipes and17 old dist rows unchanged**. No old recipe/archive filename reuse, profile/delivery setting change, path ownership/dependency direction/security default change. Relative to previousp20260929-r1, functional delta is exact source pin/provenance guard and required native QtKeychain keyring USE; charts/LibreOffice switches, all prior Qt/toolkit/desktop/KF/Sqlite/Poppler dependencies, components, install/default CMake behavior, XDG handling and optional features remain preserved.

## Archive/provenance evidence

- Pin: integrated Office **f1f3492b38e88c04ac2e2aee724c89874fdd67ea**.
- Exact source ancestry:700d85b7 installedbase and independently accepted UI02c5b4b/nativea0cfae2 all pass merge-base --is-ancestor.
- Gitlink/vendored QXlsx: **4e82d6c0726dcc39020cc5491d0de77f787c18ef**.
- Private actual source: `/home/cabewse/.cache/qindaoffice-mail-incident-20261004/dist/qindaoffice-0.1.0_p20261004.tar.xz`.
- Actual DISTDIR `/var/cache/distfiles` file is byte-identical.
- Archive size: **5,597,584bytes**.
- SHA256: **1023366d211de2d94329ad5fd8b9386f74903a980255896dcce78f1bb094bf83**.
- Manifest BLAKE2B: `0a357fc85c4df0f7ee8a45c99da8bd64bb85cd06791b9496fbe552e3b84dac341a8b73d030004092afa288575aa5464b69c609d7a8ac95589e16c7cf78b873cb`.
- Manifest SHA512: `ca5458bee6158d80e517efbc234c934c0d0dadd4836804fa6dc05bad6613cc2571aff4c4c2df5942c0e011d86d26b3203aae517a210fe1295e5dffcea0e5f5b5`.

Own independent Python source verification exits0: **2035 file members** =1280 exact Office git-archive members +754 exact QXlsx members +the exact source SHA/newline .dist-commit, plus159directories. Full file sets/bytes/types/executable bits agree; actual regular modes are0644/0755, directories0755, ownersuid/gid0, timestamp exact sourcecommit, safe root prefix/no traversal/hardlinks/embeddedGit metadata. QXlsx actual CMake source is present and every vendored tracked byte matches the exact gitlink; no build-time submodule fetch required. Inspected exact source make-dist.sh and deterministic tar/xz flags.

The first raw git-archive mode comparison exited1 because gitarchive emits0664/0775 while make-dist's staging extraction under normal022 umask yields0644/0755. Direct inspection proved identical bytes/executable bits. Corrected verification requires the tracked executable bit and exact safe actual modes; it passes. This is a verifier normalization issue, not waived source content. Manager supplied separate laptop reproduction; its same SHA is consistent with direct qinda verification, but I did not locate/re-hash that laptop file and do not claim a second reproduction.

## Portage contract evidence

Read overlay CLAUDE/README. `bash -n` exits0. Portage EAPI8 Atom/use_reduce parser exits0 for flags-off and charts+libreoffice variants (13/14 dependency atoms). `dev-libs/qtkeychain:=[keyring]` is required in every variant; private QtDBus already belongs to existing qtbase[dbus]. DEPEND mirrors RDEPEND, pkgconfig remains BDEPEND. Complete cmake eclass prepare/configure/compile/install and xdg postinst behavior is retained, rather than a partial binary/source overlay. BUILD_TESTING OFF and charts switch preserve production flags; PF-root/source archive naming, RESTRICT fetch, .dist-commit exact guard and vendored QXlsx meet network-sandbox workflow.

Exact identity/clean tree/hub ref/diff-check exit0. Actual qinda MAKEOPTS read as `-j24 -l24`, unchanged; no build needed for this recipe gate. The manager's integrated configure/build and12/12 Mail CTests remain manager evidence, not an independent rerun or artifact qualification.

## Requested next action and caveats

Root may start the full signed Portage Office build from exact6ac9a02/f1source. No install, application/session restart, authentication, user credential/settings access or signing-key access occurred. Full production build, signed artifact identity/checksum verification, staged payload/ELF provenance and eventual installation/live authentication remain separate manager/reviewer gates. I offer the next exact artifact's required-signature/public-only-keyring and payload/provenance review under the same bounded incident task. No unrelated queue work, active source build or runtime lease remains.
