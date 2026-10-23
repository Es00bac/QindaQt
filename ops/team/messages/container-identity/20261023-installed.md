# Container identity and compact applets installed on qinda-top

- Verified: 2026-10-23T17:14:34+00:00
- Authority: direct user instruction to update the installed qinda-top version. No delegated agents or external model review.
- Installed atom: `gui-wm/qindaqt-desktop-0.1.0_pre20261002-r43` (signed binary build 1).
- Exact delivery source: `3ae099948b0d6e5caa9c9d97ee5212f7a36cf232`.
- Overlay: `ab7eb97b8e833785271c38f633bc3bf371aae1a8`, integrated on the qinda hub and checkout.
- Archive: `qindaqt-desktop-0.1.0_pre20261002-r43-1.gpkg.tar`, 87,121,920 bytes, SHA-256 `40010f55123d114a17cd6533a4200a5ee647452044a585a23e76af48988c65ee`.

## Delivered scope and preserved baseline

The installed Harmony/input source `645fb4dfc` contains appearance and sleep
behavior absent from main. The isolated delivery branch applies the verified
main delta `0f2c3ca10..b783f312a` over that baseline. Fifty-seven of the sixty
changed production files are identical to verified main; Power retains Sleep
policy and its explicit modes, and the two shell-runtime files retain the live
theme catalog. Both existing packaging patches (r40 shortcut PIC and r42
Settings dialog lifetime) are applied by the new recipe. All components and SDK
files are rebuilt together; KWin stays at r7 / ABI 6.6.6.1.

The exact diff and recipe were reviewed directly, following the user's explicit
no-worker instruction. The Power compatibility file has 518 nonblank lines: the
decomposition review found one popup presentation/lifetime responsibility, no
new policy or platform ownership, and retained existing sleep controls. It stays
below the 600-line ADR threshold. The prior main-only handoff's under-500 count
still describes main, not this delivery adaptation.

The signed-image comparison with r42 preserves 2,143 entries byte-for-byte.
There are 275 changed entries (six noncompiled: task facts header, task button
QML and four switcher QML files), four added entries, and two removed entries.
The controller plugin returns from its hot-load r40 filename to the canonical
filename, with its unchanged `qindaqt_controllers` plugin ID; the other removal
is the versioned README path. Additions are the corresponding canonical plugin
and README, the container SVG and the previously omitted shortcut lease SDK
header. Existing appearance assets and configuration files are preserved.

## Verification

- Original feature evidence: 30/30 focused groups and strict native/shell builds, recorded in the October 23 source handoff.
- Delivery Power compatibility: `ctest --test-dir build/delivery-r43/power-tests -R '^qindaqt.power-applet-(controller|offscreen)$' --output-on-failure`, **2/2 groups, 14 QtTest cases, exit 0, 0.46 s**. An initial standalone fixture-path failure was fixed in test-only `eedeca03f` (main `4702bf8ff`); production source remains exactly the packaged pin.
- qinda: `emerge --oneshot --buildpkgonly --usepkg=n --getbinpkg=n '=gui-wm/qindaqt-desktop-0.1.0_pre20261002-r43'`, **exit 0**, one complete signed package.
- `pkgcheck scan --repo /var/db/repos/qindaqt '=gui-wm/qindaqt-desktop-0.1.0_pre20261002-r43'`, **exit 0**, six existing dependency-profile warnings for non-systemd/musl/x32 profiles. The target profile resolves one package with no dependency upgrade.
- Signed gpkg metadata/image verification on both hosts: **pass**, exact source/recipe and package identity.
- Packaged launcher/compositor: one 30-second-bounded private virtual 1080p test with `test_nested_session.py --expect-plugin`, **exit 0**; grouping/page isolation, rollback, detach/unwrap, output lifecycle and release pass. Its test plugin root contains only the exact packaged compositor plugin; no host controller or uinput mutation.
- qinda-top: `emerge --oneshot --usepkgonly --getbinpkg=y '=gui-wm/qindaqt-desktop-0.1.0_pre20261002-r43'`, **exit 0**, one package.
- Installed audit: **2,422/2,422 file/symlink hashes, modes and targets match the signed image; zero mismatches**. VDB confirms r43, the exact source and retained patches.
- Shell/compositor PIDs 1327/1197 and their start identities, and the observed Audio/Network/Power/Bluetooth/Settings service states/PIDs, are unchanged across installation.
- Strict MkDocs and repository link/navigation checks pass on the delivery branch (608 documents); main documentation is checked again with this installed handoff.

## Rollback, evidence and remaining boundary

The signed r42 artifact remains on qinda-top (SHA-256
`2533aade6b725d94b50995a0519a68191a3dae524045ecd5de62b5bd4095e92c`).
The target-local capture saves four existing Settings/audio/drop-in files;
services were left running and the capture is not a quiescent database backup.
The host-bound rollback script re-installs the exact signed r42 via Portage.
No saved user configuration was rewritten. Portage's pending SDDM config file
is dated October 2 and outside this desktop image; it was left untouched.

Evidence is in `build/delivery-r43/` in the laptop and qinda
`container-applets-r43-20261023` worktrees. Signed image inventories, image delta,
installed audit, package/native/test logs and process comparison are retained.
Private user-state capture stays only on its target. Unrelated dirty files in
shared checkouts are preserved; commits and overlay recipes are on qinda.

The update is installed. The user must log out and back in to adopt the new
shell/compositor; no physical login or indefinite daily-use stability is claimed.
The separate October 21 Audio1/EQ/controller-owner changes and corresponding OBS
package work remain outside this targeted delivery. Do not mark that queue or
the shared delivery catalog complete from this evidence.
