# Exact Android image Portage original-distdir repair

- Candidate: overlay486feb2b32851b682eb7f4e2703b32f2b798aaf9, from immutable29cbc1021e5c52ea5528fef7db7b87fd3cbdbe4d.
- Worktree: /home/cabewse/work_SPaC3/QindaGentoo.worktrees/everyday-android-distdir-repair-20261008; branch worker/everyday-android-distdir-repair-20261008; explicitly pushed to /home/cabewse/git/QindaGentoo.git.
- Exact changes: owning recipe, README and three additional tiny synthetic tests only. No Platform dispatcher edits.
- Original real failure: Platform526c3b5c55e278987e3f57951550fa2d2a281225; original29cbc unpack refused Portage-generated final symlinks with ELOOP before any image/package/src_test. Preserve that failure.

## Causal contract and repair

Installed Portage config.py3414–3419 exports PORTAGE_ACTUAL_DISTDIR from the configured original directory, then rewrites DISTDIR to PORTAGE_BUILDDIR/distdir. prepare_build_dirs.py519–546 creates links into the original directory. The recipe now requires an absolute existing original directory and passes it to the unchanged verifier. This is an explicit Portage implementation dependency, not a portable EAPI promise or a new untrusted path authority. The manager owns the admitted Portage configuration.

The verifier still opens the final archive O_NOFOLLOW/O_NONBLOCK, requires regular/type/size/fixed hash, reads the held descriptor and rechecks its version. The public CLI has no symlink exception. No generated link is followed; a foreign generated link is irrelevant rather than newly authorized. An original archive symlink still refuses.

Installed primary source hashes:
- config.py:2ac0285a657b4ace22140e9c923e18d7c8729aa24aa4c83c0572d20c3ff9f49c.
- prepare_build_dirs.py:32b43813cdbd10bc5737e235695f219129057c5ff4a46cc71f2cffab9a314626.

## Actual bounded checks

- PYTHONDONTWRITEBYTECODE=1 python3 app-containers/waydroid-vanilla-images/files/test_verify_images.py -v:15/15 exit0,0.008s; synthetic tiny ZIP data only.
- Three new cases: reproduce old generated-link ELOOP then admit original file; ignore foreign generated link without reading its payload; refuse mutated original bytes.
- Original12 test functions byte-identical by AST source-segment comparison.
- bash -n recipe:0; git diff --check:0.
- Manifest, verifier, provenance and PRIVATE-FEASIBILITY bytes equal29cbc.
- Raw overlay cache .cache/distdir-repair/tests.log SHA256f287a8d62bc72c02ba6b762f86d3f32dd167b2fba520ab1989e5179571774824; proof.json binds primary source and original test equality.

No real image extraction, package build, merge, guest/VM/mount or service action occurred. Request same Platform exact source review, then a separate root-granted real package attempt. Source tests do not qualify Portage unpack success.

## Continuing original Android outcome

Minimal KVM source WIP is preserved at desktop183b7f233, assigned everyday-android-stock-runtime-20261008. It is unreviewed/unexecuted: fixed QEMU input/resource boundary, fixture boot plan, guest-only stock initialization and sequential two-window observation. Rendered input and resize/close remain false. Boot/package closure and generated initramfs qualification still precede any VM grant. Stock initializer explicitly accepts block devices, permitting held readonly image disks without host mounts/filesystem exports. No custom Android image or production origin authority was introduced.
