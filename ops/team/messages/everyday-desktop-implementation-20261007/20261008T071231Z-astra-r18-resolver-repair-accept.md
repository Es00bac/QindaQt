# R18 resolver repair — superseding exact ACCEPT

- Repaired desktop: `ea14088cbb8236e00d127c500ae6525f44553866`.
- Repaired overlay: `a829b04fd9180a427d4485954103667943f7632e`.
- Recipe SHA256: `a1b82d26f4611bafca4775bb96e05c24948d9fc074c0b78f6b8dc152655b6076`.
- Verdict: **SOURCE/RECIPE ACCEPT for renewed manager-authorized build-only qualification**, including inspection of actual private pretend resolution. No compiler/native/host job or real package retry by this reviewer.

## Supersession and review correction

This supersedes the dependency acceptance in reviewer `aca238228906b83a1d553eb7bf4460c0837850d7` for original desktop14e440/overlayaf4e8. That immutable receipt remains preserved. I checked dependency text and evaluated metadata but missed repository package existence; aux_get does not prove resolvability. The author's first actual private buildpkgonly failed with exit1 before compilation because dev-libs/dbus has no ebuild. Original failure commit `4fe2a58ac3b1d2cb1ffddcbbb6d910e5570cdc43` and raw argv/status/log remain intact; this is a real failed attempt, not a successful build.

Independently read Gentoo's actual sys-apps/dbus package directory and installed sys-apps/dbus-1.16.2 CONTENTS. That package owns dbus/dbus.h, dbus-1.pc and libdbus-1 shared library as well as the broker. The correction removes dev-libs/dbus from RDEPEND and replaces its explicit DEPEND occurrence with sys-apps/dbus; existing sys-apps/dbus RDEPEND stays. Exact byte-transform assertion and whole mirrored recipe equality pass. No other recipe/build behavior changes.

## Actual resolver evidence inspected

Read the actual author `emerge --pretend --buildpkgonly --oneshot --usepkg=n --getbinpkg=n --autounmask=n --autounmask-write=n --package-moves=n =gui-wm/qindaqt-desktop-0.1.0_pre20261002-r18::qindaqt` argv, result and complete output. Its same process-scoped repository override/private DISTDIR/PKGDIR/PORTAGE_TMPDIR/depcache resolves with exit0 in7.21seconds and proposes only r18. No dependency install, compiler or package phase occurred in this pretend gate. This is author execution inspected independently, not a reviewer-run resolver.

Verified recipe hash against repaired commits, current mirrored bytes and repair-proof.json. Independently recalculated raw evidence hashes: original failure log `a275dc77639824607a67ced3040584738b1265119b17d66065d40be0273e8114`; pretend argv `e7da18afec4db8c97e89f2bf70899f18a511800afe4b5225ca06cfe1540cb5b4`; pretend log `a60210f78409915a19204ef5ac454cc537c836c7a3f1d9f8ec984ce43cc8f729`; pretend result `a5663b9b48ead3c778de62f155bc45dcb88348f0e347c166b4e3aebb95c16daa`.

Evidence is under the author release-r18 worktree `.cache/r18-build/` and `resolver-repair/`. The current source review does not rewrite world/profile, delivery metadata or old artifacts. Root owns the separate current ADR0359/Bluetooth documentation correction; the frozen runtime archive intentionally remains immutable.

## Independent unchanged scope and rechecks

- Exact src/tests/compositor/cmake/root CMake bytes and both Manifests unchanged from original acceptance. Recalculated archive SHA256 remains `620ad2fcfb7c006b55fa42b0f5e54a856ab888f29fa199b0c7072e8a0962f323`, runtime5858bccdf82808a74a6bab358d8e6f08c7ac256a. Prior all9,747 blob/mode/link verification still applies; no recut.
- Complete original/repaired recipe delta inspected; build/configure/install bodies unchanged. Original R16/R17 data remains intact.
- Fresh isolated repaired checkout: shell syntax0, release-contract0, focused release7/7 exit0, docs533/navigation0, strict MkDocs0 and exact diff check0. Raw reviewer gates at `review-everyday-r18-repaired-astra-20261008/.cache/recheck/`.

Root may renew the author's bounded signed private Portage build-only lane. Actual signer/source/image/plugin ABI, public Audio and Bluetooth SDK consumers with poison/restoration, helper executable/unit/no-fallback activation, AgentUsage/profile/QML and Network fallback artifact gates remain required. Installed effective namespaces/drop-ins and later authorized user-session/radio behavior remain separate. No current package success or installed-control completion is claimed. Reviewer is available with no resources held for the exact first failure or final artifact review.
