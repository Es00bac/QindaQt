# ACCEPT — package candidate c5cf38f0116676019a188526ebdcee056b8fe084

- Reviewer: desktop-incident-review (/root/desktop_incident_review).
- Observed: 2026-10-04T16:26:54Z.
- Exact candidate: c5cf38f0116676019a188526ebdcee056b8fe084; exact parent accepted 9ee1aa3c9f56f7a82adfb4ada6573c61dd881110.
- Exact tree: ebf9fbf335c840a3a4ffb82c430e3fb2fa74f91d.
- Detached review worktree: qinda:~/work_SPaC3/QindaGentoo-desktop-package-review-20261004.

## Exact source and recipe review

All five changed paths were inspected: README.md, both package Manifests, gui-wm/qindaqt-kwin/qindaqt-kwin-6.6.6_p1-r6.ebuild and gui-wm/qindaqt-desktop/qindaqt-desktop-0.1.0_pre20261002-r13.ebuild.

The new fork recipe is functionally identical to released r5 except exact source pin 24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae. The new desktop recipe is the full r11 source recipe, with exact source ab7fc8f3cc5cc1a5d6997e61a7d09cca475f967b and exact r6 fork dependency substituted. Independent normalized recipe comparisons pass 2/2; both shell syntax checks pass. No other dependencies, flags, security defaults, phase behavior or install paths change relative to those complete baseline recipes. The r12 signed-base/focused-compile/image-layering mechanism is absent from r13.

Direct Git ancestry check proves desktop ab7 descends from installed r12 source 2b5db406bbbbfbd1d3feb4573f64c9af1e3b298d. Its delta is only the compositor pin, incident documentation and manager board evidence; controller/keyring/physical-login source is retained. The desktop pin names accepted fork24d, tree3a257b8d991777247970c83ce9ae8ca265448db3, ABI6.6.6.1. Exactly two recipes are added, no historical recipe changed/removed, and every old Manifest record is retained with one new DIST row per package.

## Independent executable gates

| Gate | Direct result |
| --- | --- |
| Exact source Manifest validation | exit 0; 2/2 DIST filenames have exactly one row and matching file size, BLAKE2B and SHA512 |
| Actual archives versus freshly generated exact hub git archives | exit 0; file names, types, modes and SHA256 contents match: 3417 non-directory fork source entries, 9190 desktop entries |
| Actual Portage DISTDIR versus independently verified source archives | exit 0; both archives byte-identical by SHA512 |
| Desktop installed-source ancestor and exact fork pin/tree checks | pass; 2b5 is ancestor of ab7; pin24d/tree3a257/ABI6.6.6.1 |
| Normalized complete-recipe comparisons; bash -n | exit 0; 2/2 recipes retain all functional behavior except intended pins; 2/2 shell checks |
| Historical Manifest/recipe preservation | exit 0; all previous records retained; exactly 2 recipes added, no prior recipe edits/deletions |
| git diff HEAD^ HEAD --check; clean detached source tree | exit 0 / empty |
| Full actual fork GPKG signatures/checksums | exit 0 with Portage gpkg._verify_binpkg; signatures explicitly required and signature_exist asserted; signing and ignore-signature features removed in child settings |
| Fork GPKG metadata binding | exit 0; CATEGORY gui-wm, PF qindaqt-kwin-6.6.6_p1-r6, SLOT0/6.6.6_p1, BUILD_TIME1791129966; embedded recipe byte-identical to exact c5cf candidate |
| Actual stage collision/identity checker | exit 0; 456 staged files/symlinks against 1059 stock manifest paths; zero collisions, zero stock names |
| Exact checker/manifests provenance | exit 0; checker and all 3 saved stock6.6.6 manifests in actual Portage source are byte-identical to accepted fork24d |
| Actual production fork cache | exit 0; BUILD_TESTING, QINDAQT_KWIN_BUILD_TESTS and QINDAQT_SESSION_LOCK_TEST_AUTHORIZATION OFF; KWIN_NATIVE_SHORTCUTS and KWIN_BUILD_SCREENLOCKER ON |
| Actual consumer CMake cache | independently read: QindaQtKWin_DIR and QindaQtKWinDecoration_DIR select /var/tmp/portage/gui-wm/qindaqt-kwin-6.6.6_p1-r6/image/usr/lib64/cmake/; no old installed SDK selected |

The actual verified fork package is /var/cache/binpkgs/gui-wm/qindaqt-kwin/qindaqt-kwin-6.6.6_p1-r6-1.gpkg.tar, SHA256 **47c9d0618c041b954c99ded844e27c265059b7d12df8ad092054f9843d460974**.

Read-only signature verification copied only pubring.kbx/pubring.gpg when present and trustdb.gpg from Portage's configured public verification home to a disposable private home, then dropped GPG to nobody/nogroup. No signing key, passphrase, original trust bookkeeping, credential, package image extraction or installation was accessed/changed. Package metadata verification deliberately binds the actual signed artifact to this immutable recipe. The collision check supplied all three committed stock manifests because stock packages are retired; no missing-package bypass or empty manifest was used.

## Verdict boundary and requested next action

No blocking recipe/source/Manifest/fork-package finding. ACCEPT this exact overlay candidate for manager promotion. The reviewed README truthfully keeps signatures/collision/runtime/private-greeter gates distinct from focused source tests. Consumer build completion, its signed binary package and final collision/runtime/native greeter/PAM/physical adoption remain manager gates; checking its selected SDK does not imply the build passed.

Requested manager action: promote the exact accepted candidate, finish the existing consumer build and its package/contained launch checks, preserve the installed-source ancestry and explicit adoption boundary. Available next for bounded independent completed-consumer package or contained-launch evidence review for this same incident. No unrelated queue work or physical/runtime/compiler lease claimed.
