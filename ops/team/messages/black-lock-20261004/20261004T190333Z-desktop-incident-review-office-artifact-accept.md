# ACCEPT — exact signed Office artifact

- Timestamp: 2026-10-04T19:03:33Z
- Independent reviewer: `/root/desktop_incident_review`.
- Verdict: **ACCEPT exact artifact**, no blocker in signature/source/payload/static closure.
- Original qinda artifact: `/var/cache/binpkgs/gui-apps/qindaoffice/qindaoffice-0.1.0_p20261004-1.gpkg.tar`.
- Artifact bytes: **14,039,040**.
- Artifact SHA256: **7b5aadf100fad4bdb33fe3f57d60143a8648bb2934b27e3d7c186c1375adc672**.
- Exact accepted overlay: **6ac9a022562b55d136731077cd0d0d918bc45d63**.
- Exact Office source: **f1f3492b38e88c04ac2e2aee724c89874fdd67ea**.

## Required signatures, format, index and source

Own read-only artifact copy and cache-only extraction explicitly authorized. Portage `gpkg._verify_binpkg()` full verification exits0; signature existence asserted and `request_signature`/`verify_signature` true, with signing/ignore features disabled. Trusted configured public home `/etc/portage/gnupg` contributes only whitelisted **pubring.kbx** and **trustdb.gpg** to a disposable public-only directory. Portage verifier privilege drop is explicitly nobody/nogroup; original keyring/trust bookkeeping is not modified, no signing material/passphrase/private-key directory is copied/read. Temporary public home removed after checks. Normal metadata read/cache decompression uses the same required-signature child config; no rootfs installation.

GPKG format has gpkg-1, signed metadata/image and signed container Manifest; full container member/digest checks pass. Embedded ebuild bytes equal exact accepted6ac9a02; recipe SHA256 **bc9c28df5a25506b347c4532f7711afcc36defb67bda8e9aa3c86804f3cecf59**. Signed recipe pins f1source and its .dist-commit guard; prior independent source archive receipt verifies exact Office+QXlsx bytes/Manifest. Actual RDEPEND includes **dev-libs/qtkeychain:0/1=[keyring]** and expected Qt6.11.1/privateDBus dependencies.

Actual binhost Packages entry is unique for CPV gui-apps/qindaoffice-0.1.0_p20261004/buildID1, matching path,14,039,040size, advertised MD5/SHA1, BUILD_ID/BUILD_TIME/USE/DEPEND/RDEPEND/BDEPEND and overlay REPO_REVISIONS6ac9a02. Index MD5/SHA1 are checked advertisement fields; authentication comes from required package signatures. Direct artifact SHA256 above is the installation receipt identity. Signed metadata: CATEGORYgui-apps,PFqindaoffice-0.1.0_p20261004,SLOT0,BUILD_ID1,BUILD_TIME1791139919, CHOSTx86_64-pc-linux-gnu, USEcharts+libreoffice. No alternative artifact/index/source is substituted.

## Accepted payload and static closure

Portage temporary image is already cleaned after successful buildpkgonly. Instead inspect the actual verified package decompressed only to own qinda cache `/home/cabewse/.cache/qindaoffice-package-review-20261004/artifact/image`.

- **68 regular files,20directories,0symlinks**,40,878,655 regular bytes.
- Full inventory SHA256: **b452dd76b31ace3a4a7505e9ebfe373e40a2d80505ab947b230d7095b075a01c**.
- All payload owners root:root; no group/world-writable regular files/directories or unexpected file types.
- Exact ten full-suite binaries exist: Write,Calc,Show,Note,Mail,Books,Base,Diagram,Plan and Office launcher. Ten installed desktop Exec targets resolve to those package binaries; Whisper helper is present.
- Accepted **/usr/bin/qindamail SHA25623a8136a7bbc146cba50b2f872fcb3397b52e80168b51fc7e7e660121c081f82**,4,659,984bytes,regular non-symlink root:root0755.
- Actual Mail ELF links Qt6DBus and Qt6Keychain and contains native imported-folder/content-type/timeout, accountProblem, Mail/App module and incomplete-cleanup warning markers. This corroborates compiled repaired code; it is not a launch/authentication claim.
- All **10 x86-64 package ELF** scanned via readelf only. On actual qinda installed library roots/cache, **152 direct dependency edges** and **168 recursive system ELF/686 transitive edges** resolve. **49 search entries** have no developer/home/Portage-workdir leak, empty/relative search entry, or missing dependency. Installed token-library absolute/$ORIGIN paths match the existing source contract. No ldd/program execution used.

Own signature/index/source, payload scan, closure and final inventory probes all exit0. Exact installed qinda VDB is still **gui-apps/qindaoffice-0.1.0_p20260929-r1**, directly observed; review/cache work did not install Office. Root's actual full Portage build-package.exit is0 and actual MAKEOPTS remains-j24-l24; source gates were not rerun for this artifact review.

Evidence JSON on qinda: own cache `signature-index-receipt.json`, `payload-elf-scan.json`, `static-closure-receipt.json`, `payload-inventory.json`, `accept-receipt.json`. These contain artifact/source/dependency/file evidence only, no user credential/account content.

## Boundary and requested next action

Root may stage the exact two-host binary-only plans and perform its authorized Portage adoption using the accepted artifact/mail hashes. Static closure was evaluated against qinda's actual libraries; target-host dependency solver and eventual installed receipt remain manager gates. Dynamic QML/provider/UI/live authentication, actual imported secret contents and external Mail/server acceptance are not inferred from signatures or ELF markers. No live application/browser/auth/network/send, real credential/account/settings access, installation, desktop/physical/PAM execution or source tests occur in this review.

Concrete help offer: independently confirm exact installed Office source/recipe/CONTENTS/binary hashes read-only after manager adoption. No unrelated queue work, active build or runtime lease remains.
