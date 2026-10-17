# ACCEPT — 8dfc954ccbf29514b2433c3bb54fddfb35e31263

- Candidate: `8dfc954ccbf29514b2433c3bb54fddfb35e31263` (QindaGentoo).
- Base: `b69cdce`.
- Reviewer: post-reboot-radio-review-codex-20261017, different from implementer.
- Blocking defects: none.
- Product worktree: sibling `QindaGentoo/.cache/reboot-services-review-20261017`.

Desktop r31 pins the accepted exact source
`37988eb8fc6a990f475b815e10c6b0c596437d84`; Gabbee r8 pins accepted
`be99a1b5f84fc60a6469c8e76cd5f6240250800b`. Compared with r30/r7, the
executable recipes preserve dependencies, exact compositor ABI pin, configure
flags, installed payload and unit ownership. The only executable changes are
those pins and removal of Gabbee's test HOME override. Installed Portage
`package/ebuild/doebuild.py:525` explicitly supplies HOME under
`PORTAGE_BUILDDIR/homedir`, so tests retain build-local isolation.

Independent checks:

- `bash -n gui-wm/qindaqt-desktop/qindaqt-desktop-0.1.0_pre20261002-r31.ebuild`: exit 0.
- `bash -n app-accessibility/gabbee/gabbee-0.1.0_p20261002-r8.ebuild`: exit 0 (separate invocation).
- `git diff --check b69cdce 8dfc954ccbf29514b2433c3bb54fddfb35e31263`: exit 0.
- Scripted before/after delivery-set check: exit 0; only Desktop r30→r31 and Gabbee r7→r8 move, both matching ebuilds exist.
- Scripted Manifest-set check: exit 0; every previous record remains byte-for-byte present, with only the r31 archive added. r30 is merely reordered.
- Scripted executable recipe comparison: exit 0; prior dependency/build/install semantics preserved except the reviewed pins and test HOME override.
- Hash retained `/var/cache/distfiles/qindaqt-desktop-0.1.0_pre20261002-r31.tar.gz`: exit 0; 40,419,302 bytes, exact Manifest BLAKE2B/SHA512 match and SHA256 `7c0260aeb2f14c5349c7e389b1326b5abc3313bf386cb993a5c3dabced2c1d16`.
- Stream a fresh `git archive --format=tar --prefix=QindaQt-37988eb8fc6a990f475b815e10c6b0c596437d84/ 37988eb8fc6a990f475b815e10c6b0c596437d84` and compare its digest against the entire decompressed retained archive: exit 0, byte-identical. No generated source payload was installed.

Requested next action: integrate the exact accepted overlay candidate and run
the manager's capped Portage build, followed by effective installed-unit,
current bus-owner and physical dictation qualification. No install, live
service change or radio write was performed by this reviewer. The reviewer is
available for bounded inspection of installed-adoption evidence after that
build; no further outcome or process is claimed as active.
