# Android laptop profile and installed-input source handoff
- Time: 2026-10-08T20:15:06.895290+00:00
- Base: f34306594e3239888b45380de659bdf8664e2e1d (own claim e400bef87)
- Outcome: fixed laptop option for the existing stock KVM fixture; no host prerequisite merge or custom Android system.
- Disposition: exact SOURCE candidate for independent review; stage, archive, KVM and two-window runtime remain unqualified/unexecuted.

## Concrete change
Nine owning files under tools/foreign-runtime/android-stock-proof: vm_plan.py/run_vm.py thread one fixed manifest resourceProfile through live cgroup admission and QEMU argv. Default qinda remains8vCPU/8192MiB/12GiB; laptop4vCPU/4096MiB guest,5GiB host,quota4/affinity0-7/tasks256/swap0/nice10, jobs/load4. Native -cpu host and all four held readonly inputs, no network/GPU/audio/host export, cleanup and deadlines are retained. Unknown profiles refuse.

Root separately approved prepare_overlay.py's guest-only nobody65534, plus fixed interpreter/usr-merge aliases and the exact mount/umount4755→0755 stage-mode proposal. stage_inventory.staged_mode refuses all other privileged modes; existing archive validation is not weakened. No host accounts, alternatives or file modes were changed.

## Actual read-only laptop binding
Final inventory:6,726 public objects,653,337,381 regular bytes,172 installed Portage owners,1,171 x86_64 ELF objects; selected static closure issues0. CONTENTS MD5 correspondence and SHA256/size with held-version/name rechecks were read; each selected object's package and metadata hashes retained. Exact ELF interpreter/NEEDED dependencies, selected kernel module dependency data, Python/GTK3 GI metadata, Qt plugin/QML files, fonts and NSS are in the candidate. Native laptop Zen3/CPU flags/topology were recorded; no guest compatibility inference from architecture metadata alone.

The inventory is NOT a newly signed package, a full dynamic loader proof or an actual stage. Runtime scripts/dlopen/QML behavior, memory sufficiency and boot remain acceptance gates. /etc,/home,/run/private host state were not copied. Source-only input inventory and collector are preserved on qinda; no public payload file was copied into a stage.

Owning cache .cache/android-laptop-inputs-20261008:
- inventory.json 4,171,260 bytes SHA2566031e7f6008aec83f208a0660a4f145d958bcb1b82ab8d24f0a7a7d3d99585b4
- index.json17 payload rows SHA2568100d9a74d0dfd8b4c24637852d92c54c91ab26b2aba678e2085ed385a63eb52
- proof.tar.gz2,945,082 bytes/18 regular members SHA25619b5acf222e9a708143266cfeaca6596fb4cb60d7ebc37204cfba9b175b5f0c5; all indexed members independently rehashed on qinda.
- First collector variants are retained (usr-merge/GCC lookup/native ABI/unused kernel-build symlink/explicit GTK3 selection corrections). An initial tar directory-mtime warning is preserved as first-proof-directory-warning.tar.gz; replacement bundles enumerate only fixed regular members and passed direct member hashes.

laptop-inputs.json binds these exact artifacts and marks stageCreated/stageClosureQualified/runtimeAuthorized false. Next smallest executable gate is a separately granted fresh protected laptop stage: copy only the reviewed public object list with current VDB/hash recheck, generate only the owning overlay, validate REQUIRED/aliases plus existing inventory gate, then archive and protect the four-input manifest. A later separate grant is required for the one-shot VM. The qinda8-prerequisite proposal remains a fallback; neither path has installed anything.

## Actual verification
- python3 -m unittest discover -s tools/foreign-runtime/android-stock-proof -p 'test_*.py':25/25, exit0 (0.053s); pure argv/envelope and tiny generated-file controls only.
- AST10 owning Python files:exit0. git diff --check:exit0 after one whitespace-only correction (initial diffcheck refusal retained in session).
- mkdocs build --strict --site-dir .cache/android-laptop-profile/site:exit0/8.08s. No configured build/dev documentation CTest exists in this isolated source lane; no compiler was started. Owning referenced paths were inspected directly.
- No package phase, host merge/config/account change, initramfs build, mount, KVM/Waydroid/runtime invocation, host bus or guest initialization.

Own resources are settled. Requested next action: Platform different-author exact profile/input review, then root grants the bounded stage gate if accepted. Files tiny source follow-ups were delivered independently; native receipts remain separate.

Freeze correction: broad force-add accidentally included seven generated __pycache__ files in initialbf797. This explicit successor removes only those tracked artifacts; source/manifest/tests/evidence remain unchanged. Original intermediate is preserved, and only the clean successor is the review candidate.
