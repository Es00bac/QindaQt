# ACCEPT — exact native locker PAM policy and unlock regression

- Timestamp: 2026-10-07T14:53:22+00:00
- Reviewer: Incident Policy Reviewer Codex
- Overlay candidate: e62153bc6b1ed54fbda97e63c70bc918e8588bb7
- Overlay base: 62d4ca4415e73bab95ae53abe6855dc305ecf693
- Unlock candidate: 1297b81ad9b832d4fe4d152aa8ecd344950a010d
- Product base: 46e6a74dc0de6b279ca8c2a24e50d3f86634d925
- Signed artifact SHA256: 5eeb37dbd90bb34c31b56183701c1e84f40692bb33287a5007fddba11d8cc589

## Verdict and causal boundary

ACCEPT all three exact boundaries above. No blocking finding. The configured authentication/account substacks retain Gentoo site authority, including the inspected required pam_unix/faillock checks. Optional native keyring observation cannot grant unlock or veto approved authentication. Password changes are denied; session contains only pam_permit and optional keyring, excluding login/logind session creation. Current worker source approves authentication and account before invoking its optional session notification. The real-UID identity and PAM_DISALLOW_NULL_AUTHTOK contract are preserved.

The new package owns the missing fixed qindaqt-lock service. Profile/category/keyword/exact delivery integration is additive and old recipes/archive records remain unchanged. Its pambase/PAM dependencies and pam.eclass newpamd implementation are appropriate for a configuration-only EAPI8 package. The no-distfile thin repository correctly generates no Manifest file. The README explicitly requires future desktop snapshots to depend on the service; Program Manager owns that upcoming recipe integration.

The regression's private-confdir other/pam_deny stack emits Result/Denied as its first frame, proving no credential conversation. Existing positive and account-denial rows establish that the intended worker protocol requires prompt plus authentication/account success. Documentation accurately distinguishes launch/role/frame evidence from prompt readiness and actual physical-session authentication.

## Independent executable evidence

All commands executed on qinda, with no host authentication or installation.

- ebuild exact review-tree recipe manifest: exit0; no Manifest generated because thin package has no distfiles. bash -n, metadata XML parse and both exact diff --check commands: exit0.
- Installed Linux-PAM pam_start_confdir over private temporary configurations and nonexistent synthetic account: 4/4 rows, exit0. Exact policy bytes accept private permit auth/account; required auth denial returns7; required account denial returns7; every password change returns20; configured session returns0; missing service returns7 with zero conversation calls and denied session14. No owner credential or live system-auth authenticator is executed by this matrix.
- Exact candidate-built authentication binaries independently rerun with QT_FATAL_WARNINGS=1: CTests3/3, Qt checks37 (12+10+15), zero failure/skip. Auth source in the worker tree matches immutable1297; cache source path and new test function were inspected, binary hashes recorded. This reviewer reran the already built binaries; no independent compiler build is claimed.
- mkdocs build --strict in review tree: exit0. python3 tools/docs_validation.py: exit0,510 Markdown documents/navigation.
- Portage gpkg verifier with signature required, ignoring disabled, metadata_only=False: exit0; validates complete signed Manifest, metadata and image.

## Exact artifact inspection

`/var/cache/binpkgs/sys-auth/qindaqt-lock-pam/qindaqt-lock-pam-1-1.gpkg.tar` matches the declared SHA256. Metadata identifies sys-auth/qindaqt-lock-pam-1, repository qindaqt, EAPI8, GPL-3+, RDEPEND sys-auth/pambase sys-libs/pam, with no DEPEND/BDEPEND/PDEPEND. Its image has root0755 ancestor directories and exactly one regular file: `/etc/pam.d/qindaqt-lock`, root:root0644,486 bytes, source-identical SHA256 `f99cace68cc32277ed0f26d8757e8496a472dff25d9c9f6a73792365483400d5`. No executable, unit, symlink or additional configuration is included.

Ignored evidence lives in review `build/incident-policy-review/`: pam-stack-semantics.json, pam-artifact-inspection.json, unlock-focused-ctest.log, unlock-test-binaries.json and docs logs. These are retained qinda evidence, not production source.

## Remaining gate and next action

Program Manager may proceed with the user's authorized only-policy signed Portage installation. Source/configuration tests do not claim visible physical keyboard focus, real password approval or actual screen unlock. Owner-controlled physical authentication remains the bounded live acceptance gate.

Concrete help offer after queue/peer reread: review Battery Startup Codex's exact candidate, then updated desktop runtime dependency and signed artifact, without compiler/private compositor ownership unless needed and assigned.

Control semantics reference: [Linux-PAM configuration](https://github.com/linux-pam/linux-pam/blob/master/doc/man/pam.conf-syntax.xml); installation interface: [Gentoo pam.eclass](https://devmanual.gentoo.org/eclass-reference/pam.eclass/index.html). Direct private matrix and installed eclass inspection supply the acceptance evidence.
