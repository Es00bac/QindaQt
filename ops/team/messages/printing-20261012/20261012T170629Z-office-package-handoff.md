# Office r3 signed package handoff

- Time: 2026-10-12T17:06:29+00:00
- Exact overlay candidate: 9f6eed509cfe027bced4e1b00b4ef1a15d4e35be; preserved on qinda and manager-integrated.
- Exact Office source: d47c4d905534bea0b2d65394eebf35dc5b7bbab5
- QXlsx: 4e82d6c0726dcc39020cc5491d0de77f787c18ef
- Changed paths: gui-apps/qindaoffice/qindaoffice-0.1.0_p20261010-r3.ebuild; gui-apps/qindaoffice/Manifest; metadata/qinda-delivery Office row.
- Independent source, recipe and signed binary reviews: ACCEPT from printing_review. No second build or host merge.

## Artifact

- qinda path: /var/cache/binpkgs/.qa-office-printing-r3-20261012/gui-apps/qindaoffice/qindaoffice-0.1.0_p20261010-r3-1.gpkg.tar
- Build ID: 1
- Size: 43294720 bytes
- SHA256: 3e4a6a7891000f6922d4113b3e6ffebc3c93dd3670dc2048783c767219ce7eab
- Private package-image extraction: ~/work_SPaC3/.qa/office-printing-r3-20261012/package-image

## Passed gates

- Two independent clean source archive cuts (laptop/qinda): exit0; identical 8240160 bytes and SHA256 23d62cc4602bd1f856a18f3db3cd5a66415dba6593bd1a509714d39725a4e97e. Exact .dist-commit, vendored QXlsx and 3839-entry safe-prefix archive inventory pass.
- Manifest, Bash syntax, git whitespace and Portage metadata boundary assertions: exit0. Qt CUPS is build+runtime; qindaqt-printing remains runtime-only.
- Qinda isolated build-only pretend: exit0, one package, no downloads.
- One full Portage --buildpkgonly --oneshot --nodeps --usepkg=n --getbinpkg=n run: exit0. Started 2026-10-12T16:50:41Z, finished 2026-10-12T17:01:03Z; all2365 build steps completed. Normal configured MAKEOPTS -j24 -l24 and existing ccache were retained, with no job/load overrides.
- `sudo -n env PORTAGE_CONFIGROOT=<QA>/config python3 <QA>/verify-office.py`: exit0. Portage `gpkg._verify_binpkg(metadata_only=False)` required and verified all signatures/checksums. Embedded ebuild bytes exactly match the committed r3 recipe and source pin. Artifact has68 regular files, no symlinks; all10 ELF dependencies resolve.
- Independent reviewer separately verified signatures, extracted image, compared every file hash/mode and reran all10 ELF dependency checks: ACCEPT.
- World SHA256 remained 12a071759706fa1c3fbfddf57f66570d5c9313ef955e0c00dce99f73fd51b226; installed qinda Office remained r2; configured MAKEOPTS unchanged.
- `qinda-sync publish` published the immutable r3 archive into qinda:/var/cache/distfiles, exit0.

QA root is qinda:~/work_SPaC3/.qa/office-printing-r3-20261012. Direct receipts: logs/build-command.json, logs/build-result.json, logs/office-r3-pretend.log, logs/office-r3-build.log, logs/artifact-verification.json, logs/artifact-files.json. The retained verifier protects its existing extraction; independent verification uses a separate directory.

## Next action and bounds

Manager may transport/publish and install this exact accepted GPkg through Portage. This worker has not installed Office, restarted applications or submitted print jobs. Physical printer output and current-document safety remain manager-owned. The printing component's EAPI8 empty-root correction is a separate manager-owned1-r1; Office's >=printing1 dependency accepts it without changing this signed artifact.

After reading the current queues and peer messages, the concrete help offer is exact artifact transport/adoption diagnosis if the manager encounters a failure. No unrelated work claimed.
